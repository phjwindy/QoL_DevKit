// ---- continuous native-save backups ---------------------------------------
//
// This module never writes to the live save directory.  The existing mod
// worker calls InitializeSaveBackup() after the package gate is open and then
// calls PumpSaveBackup() from its ordinary background loop.  Game-main-thread
// hooks only publish the pointer-free CGameTime snapshot consumed below.
//
// Live source:
//   %APPDATA%\Nippon Ichi Software, Inc\Honogurashinoniwa\<SteamID64>
//
// Owned destination:
//   <game directory>\Village_Save_Backups\<SteamID64>\Day_NNNNNNNNNN\...
//
// A snapshot is published only after the complete live-file set has been
// unchanged for two seconds, survives source SHA-256 pass A, has been copied
// into a marked partial directory, and then has target/source-pass-B hashes
// identical to pass A while the directory fingerprint still matches. Renaming
// the partial directory publishes it atomically on the same volume. Cleanup
// refuses reparse points,
// unknown names, missing ownership markers and every path outside our profile
// backup root.

static constexpr size_t SAVE_BACKUP_PATH_CAP = 2048;
static constexpr size_t SAVE_BACKUP_MAX_FILES = 102;
static constexpr size_t SAVE_BACKUP_MAX_DAYS = 128;
static constexpr ULONGLONG SAVE_BACKUP_POLL_MS = 250;
static constexpr ULONGLONG SAVE_BACKUP_STABLE_MS = 2000;
static constexpr ULONGLONG SAVE_BACKUP_RETRY_MS = 1000;
static constexpr ULONGLONG SAVE_BACKUP_ACCOUNT_RETRY_MS = 2000;
static constexpr ULONGLONG SAVE_BACKUP_MAX_FILE_SIZE = 64ULL * 1024ULL * 1024ULL;
static constexpr ULONGLONG SAVE_BACKUP_MAX_TOTAL_SIZE = 512ULL * 1024ULL * 1024ULL;
static constexpr u64 SAVE_BACKUP_STEAM_ID64_BASE = 76561197960265728ULL;
static constexpr u64 SAVE_BACKUP_SECONDS_PER_DAY = 86400ULL;
static constexpr u64 SAVE_BACKUP_SECONDS_PER_HOUR = 3600ULL;

static constexpr wchar_t SAVE_BACKUP_DIRECTORY_NAME[] = L"Village_Save_Backups";
static constexpr wchar_t SAVE_BACKUP_FINAL_MARKER_NAME[] =
    L".village_qol_backup";
static constexpr wchar_t SAVE_BACKUP_PARTIAL_MARKER_NAME[] =
    L".village_qol_partial";
static constexpr wchar_t SAVE_BACKUP_DAY_MARKER_NAME[] =
    L".village_qol_day";
static constexpr char SAVE_BACKUP_FINAL_MARKER[] =
    "VILLAGE_QOL_SAVE_BACKUP_V1\n";
static constexpr char SAVE_BACKUP_PARTIAL_MARKER[] =
    "VILLAGE_QOL_SAVE_BACKUP_PARTIAL_V1\n";
static constexpr char SAVE_BACKUP_DAY_MARKER_PREFIX[] =
    "VILLAGE_QOL_SAVE_BACKUP_DAY_V1 ";

enum class SaveBackupCategory : unsigned {
    Dawn = 0,
    Midnight = 1,
    Other = 2,
};

enum class SaveBackupCaptureResult : unsigned {
    Failed = 0,
    SourceChanged = 1,
    Published = 2,
};

struct SaveBackupFileIdentity {
    wchar_t name[16];
    ULONGLONG size;
    ULONGLONG lastWrite;
};

struct SaveBackupFileSet {
    SaveBackupFileIdentity files[SAVE_BACKUP_MAX_FILES];
    size_t count;
    ULONGLONG totalSize;
};

struct SaveBackupHashIdentity {
    unsigned char sha256[32];
    unsigned size;
};

struct SaveBackupHashSet {
    SaveBackupHashIdentity files[SAVE_BACKUP_MAX_FILES];
    size_t count;
};

struct SaveBackupClockSnapshot {
    std::int64_t rawSecond;
    u64 worldEpoch;
    u64 loadGeneration;
    u64 dayId;
    unsigned hour;
    unsigned minute;
    unsigned second;
};

struct SaveBackupDayCandidate {
    wchar_t name[48];
    ULONGLONG touched;
};

static bool g_saveBackupInitialized = false;
static bool g_saveBackupSourceReady = false;
static bool g_saveBackupDirty = false;
static bool g_saveBackupSourceMissingLogged = false;
static ULONGLONG g_saveBackupNextPollAt = 0;
static ULONGLONG g_saveBackupStableSince = 0;
static ULONGLONG g_saveBackupNextAccountRetryAt = 0;
static unsigned g_saveBackupSequence = 0;
static wchar_t g_saveBackupRoot[SAVE_BACKUP_PATH_CAP] = {};
static wchar_t g_saveBackupProfileRoot[SAVE_BACKUP_PATH_CAP] = {};
static wchar_t g_saveBackupLiveRoot[SAVE_BACKUP_PATH_CAP] = {};
static wchar_t g_saveBackupSteamId[32] = {};
static SaveBackupFileSet g_saveBackupObserved = {};

static ULONGLONG SaveBackupFileTimeValue(FILETIME value) {
    ULARGE_INTEGER converted = {};
    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return converted.QuadPart;
}

static bool SaveBackupJoinPath(wchar_t* output, size_t capacity,
                               const wchar_t* left, const wchar_t* right) {
    if (!output || capacity == 0 || !left || !right || !left[0] || !right[0]) {
        return false;
    }
    const size_t leftLength = wcslen(left);
    const size_t rightLength = wcslen(right);
    const bool separatorNeeded = left[leftLength - 1] != L'\\' &&
                                 left[leftLength - 1] != L'/';
    if (leftLength + (separatorNeeded ? 1 : 0) + rightLength + 1 > capacity) {
        return false;
    }
    memcpy(output, left, leftLength * sizeof(wchar_t));
    size_t cursor = leftLength;
    if (separatorNeeded) output[cursor++] = L'\\';
    memcpy(output + cursor, right, (rightLength + 1) * sizeof(wchar_t));
    return true;
}

static bool SaveBackupDirectoryIsPlain(const wchar_t* path) {
    if (!path || !path[0]) return false;
    const DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

static bool SaveBackupEnsureDirectory(const wchar_t* path) {
    if (!path || !path[0]) return false;
    if (!CreateDirectoryW(path, nullptr)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
    }
    return SaveBackupDirectoryIsPlain(path);
}

static bool SaveBackupPathIsUnderProfile(const wchar_t* path) {
    if (!path || !path[0] || !g_saveBackupProfileRoot[0]) return false;
    const size_t rootLength = wcslen(g_saveBackupProfileRoot);
    return _wcsnicmp(path, g_saveBackupProfileRoot, rootLength) == 0 &&
           path[rootLength] == L'\\' && path[rootLength + 1] != L'\0';
}

static bool SaveBackupWriteBytes(const wchar_t* path, const void* bytes,
                                 DWORD byteCount) {
    if (!path || !bytes || byteCount == 0) return false;
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes, byteCount, &written, nullptr) &&
                    written == byteCount && FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) DeleteFileW(path);
    return ok;
}

static bool SaveBackupWriteUtf8(const wchar_t* path, const wchar_t* text) {
    if (!path || !text) return false;
    const int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                            text, -1, nullptr, 0,
                                            nullptr, nullptr);
    if (needed <= 1 || needed > 30000) return false;
    char buffer[30003] = {};
    buffer[0] = static_cast<char>(0xef);
    buffer[1] = static_cast<char>(0xbb);
    buffer[2] = static_cast<char>(0xbf);
    const int converted = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, buffer + 3,
        static_cast<int>(sizeof(buffer) - 3), nullptr, nullptr);
    if (converted != needed) return false;
    return SaveBackupWriteBytes(
        path, buffer, static_cast<DWORD>(3 + converted - 1));
}

static bool SaveBackupReadSmallFile(const wchar_t* path, char* output,
                                    size_t capacity, DWORD* usedOut) {
    if (usedOut) *usedOut = 0;
    if (!path || !output || capacity < 2 || capacity > 0xffffffffULL) {
        return false;
    }
    HANDLE file = CreateFileW(path, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size = {};
    const bool sizeOk = GetFileSizeEx(file, &size) && size.QuadPart >= 0 &&
                        static_cast<ULONGLONG>(size.QuadPart) < capacity;
    DWORD used = 0;
    const bool ok = sizeOk &&
        ReadFile(file, output, static_cast<DWORD>(size.QuadPart), &used,
                 nullptr) && used == static_cast<DWORD>(size.QuadPart);
    CloseHandle(file);
    if (!ok) return false;
    output[used] = '\0';
    if (usedOut) *usedOut = used;
    return true;
}

static bool SaveBackupMarkerEquals(const wchar_t* directory,
                                   const wchar_t* markerName,
                                   const char* expected,
                                   bool prefixOnly = false) {
    wchar_t path[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(path, _countof(path), directory, markerName)) {
        return false;
    }
    char content[160] = {};
    DWORD used = 0;
    if (!SaveBackupReadSmallFile(path, content, sizeof(content), &used)) {
        return false;
    }
    const size_t expectedLength = strlen(expected);
    return prefixOnly
        ? used >= expectedLength &&
              memcmp(content, expected, expectedLength) == 0
        : used == expectedLength &&
              memcmp(content, expected, expectedLength) == 0;
}

static bool SaveBackupIsLiveFileName(const wchar_t* name) {
    if (!name || !name[0]) return false;
    if (_wcsicmp(name, L".systemsave") == 0 ||
        _wcsicmp(name, L"save.lst") == 0) return true;
    if (wcslen(name) != 8 || _wcsnicmp(name, L"save.", 5) != 0) {
        return false;
    }
    return name[5] >= L'0' && name[5] <= L'9' &&
           name[6] >= L'0' && name[6] <= L'9' &&
           name[7] >= L'0' && name[7] <= L'9';
}

static bool SaveBackupSnapshotEntryIsOwned(const wchar_t* name) {
    return SaveBackupIsLiveFileName(name) ||
           _wcsicmp(name, L"manifest.txt") == 0 ||
           _wcsicmp(name, SAVE_BACKUP_FINAL_MARKER_NAME) == 0 ||
           _wcsicmp(name, SAVE_BACKUP_PARTIAL_MARKER_NAME) == 0;
}

static bool SaveBackupFileSetEqual(const SaveBackupFileSet& left,
                                   const SaveBackupFileSet& right) {
    if (left.count != right.count || left.totalSize != right.totalSize) {
        return false;
    }
    for (size_t index = 0; index < left.count; ++index) {
        if (_wcsicmp(left.files[index].name,
                     right.files[index].name) != 0 ||
            left.files[index].size != right.files[index].size ||
            left.files[index].lastWrite != right.files[index].lastWrite) {
            return false;
        }
    }
    return true;
}

static bool SaveBackupHashFileSet(const wchar_t* directory,
                                  const SaveBackupFileSet& files,
                                  SaveBackupHashSet* output) {
    if (!directory || !output || files.count > SAVE_BACKUP_MAX_FILES ||
        !SaveBackupDirectoryIsPlain(directory)) return false;
    *output = {};
    for (size_t index = 0; index < files.count; ++index) {
        wchar_t path[SAVE_BACKUP_PATH_CAP] = {};
        if (!SaveBackupJoinPath(path, _countof(path), directory,
                                files.files[index].name)) return false;
        const DWORD attributes = GetFileAttributesW(path);
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return false;
        DWORD error = ERROR_SUCCESS;
        unsigned size = 0;
        if (!PackageHashFile(path, output->files[index].sha256,
                             &size, &error) ||
            static_cast<ULONGLONG>(size) != files.files[index].size) {
            SetLastError(error != ERROR_SUCCESS ? error : ERROR_FILE_INVALID);
            return false;
        }
        output->files[index].size = size;
        ++output->count;
    }
    return output->count == files.count;
}

static bool SaveBackupHashSetEqual(const SaveBackupHashSet& left,
                                   const SaveBackupHashSet& right) {
    if (left.count != right.count) return false;
    for (size_t index = 0; index < left.count; ++index) {
        if (left.files[index].size != right.files[index].size ||
            memcmp(left.files[index].sha256,
                   right.files[index].sha256, 32) != 0) return false;
    }
    return true;
}

static void SaveBackupSha256Text(const unsigned char digest[32],
                                 char output[65]) {
    static constexpr char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < 32; ++index) {
        output[index * 2] = digits[digest[index] >> 4];
        output[index * 2 + 1] = digits[digest[index] & 0x0f];
    }
    output[64] = '\0';
}

static bool SaveBackupEnumerateLiveFiles(const wchar_t* directory,
                                         SaveBackupFileSet* output) {
    if (!directory || !output || !SaveBackupDirectoryIsPlain(directory)) {
        return false;
    }
    *output = {};
    wchar_t pattern[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(pattern, _countof(pattern), directory, L"*")) {
        return false;
    }
    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    bool hasSystem = false;
    bool hasList = false;
    bool hasSlot = false;
    do {
        if (!SaveBackupIsLiveFileName(found.cFileName)) continue;
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
            output->count >= SAVE_BACKUP_MAX_FILES) {
            ok = false;
            break;
        }
        ULARGE_INTEGER size = {};
        size.LowPart = found.nFileSizeLow;
        size.HighPart = found.nFileSizeHigh;
        if (size.QuadPart < 0x14 || size.QuadPart > SAVE_BACKUP_MAX_FILE_SIZE ||
            output->totalSize > SAVE_BACKUP_MAX_TOTAL_SIZE - size.QuadPart) {
            ok = false;
            break;
        }
        SaveBackupFileIdentity entry = {};
        if (wcslen(found.cFileName) >= _countof(entry.name)) {
            ok = false;
            break;
        }
        wcscpy_s(entry.name, found.cFileName);
        entry.size = size.QuadPart;
        entry.lastWrite = SaveBackupFileTimeValue(found.ftLastWriteTime);
        size_t insert = output->count;
        while (insert > 0 &&
               _wcsicmp(output->files[insert - 1].name, entry.name) > 0) {
            output->files[insert] = output->files[insert - 1];
            --insert;
        }
        if ((insert > 0 &&
             _wcsicmp(output->files[insert - 1].name, entry.name) == 0) ||
            (insert < output->count &&
             _wcsicmp(output->files[insert].name, entry.name) == 0)) {
            ok = false;
            break;
        }
        output->files[insert] = entry;
        ++output->count;
        output->totalSize += entry.size;
        hasSystem = hasSystem || _wcsicmp(entry.name, L".systemsave") == 0;
        hasList = hasList || _wcsicmp(entry.name, L"save.lst") == 0;
        hasSlot = hasSlot || (wcslen(entry.name) == 8 &&
                              _wcsnicmp(entry.name, L"save.", 5) == 0);
    } while (FindNextFileW(search, &found));
    const DWORD finalError = GetLastError();
    FindClose(search);
    return ok && finalError == ERROR_NO_MORE_FILES && hasSystem && hasList &&
           hasSlot && output->count >= 3;
}

static bool SaveBackupReadContainerMagic(const wchar_t* path) {
    static constexpr char expected[] = "YKCMP_V1";
    HANDLE file = CreateFileW(path, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                  FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    char magic[sizeof(expected) - 1] = {};
    DWORD read = 0;
    const bool ok = ReadFile(file, magic, sizeof(magic), &read, nullptr) &&
                    read == sizeof(magic) &&
                    memcmp(magic, expected, sizeof(magic)) == 0;
    CloseHandle(file);
    return ok;
}

// SaveBackupReadClock — 直接读取游戏时钟（替代旧版原子变量发布机制）
static bool SaveBackupReadClock(SaveBackupClockSnapshot* output) {
    if (!output) return false;
    const std::int64_t rawSecond = ReadGameRawSecond();
    if (rawSecond < 0) return false;
    const u64 raw = static_cast<u64>(rawSecond);
    const u64 displaySecond =
        (raw % SAVE_BACKUP_SECONDS_PER_DAY +
         7ULL * SAVE_BACKUP_SECONDS_PER_HOUR) %
        SAVE_BACKUP_SECONDS_PER_DAY;
    output->rawSecond = rawSecond;
    output->worldEpoch = 1;
    output->loadGeneration = 0;
    output->dayId = raw / SAVE_BACKUP_SECONDS_PER_DAY;
    output->hour = static_cast<unsigned>(
        displaySecond / SAVE_BACKUP_SECONDS_PER_HOUR);
    output->minute = static_cast<unsigned>(
        (displaySecond % SAVE_BACKUP_SECONDS_PER_HOUR) / 60ULL);
    output->second = static_cast<unsigned>(displaySecond % 60ULL);
    return true;
}

static SaveBackupCategory SaveBackupClassify(
        const SaveBackupClockSnapshot& clock) {
    if (clock.hour < 6) return SaveBackupCategory::Midnight;
    if (clock.hour < 12) return SaveBackupCategory::Dawn;
    return SaveBackupCategory::Other;
}

static const wchar_t* SaveBackupCategoryName(SaveBackupCategory category) {
    switch (category) {
    case SaveBackupCategory::Dawn: return L"DAWN";
    case SaveBackupCategory::Midnight: return L"MIDNIGHT";
    default: return L"OTHER";
    }
}

static const char* SaveBackupCategoryNameAscii(
        SaveBackupCategory category) {
    switch (category) {
    case SaveBackupCategory::Dawn: return "DAWN";
    case SaveBackupCategory::Midnight: return "MIDNIGHT";
    default: return "OTHER";
    }
}

static bool SaveBackupReadSteamAccountId(u64* accountIdOut) {
    if (!accountIdOut) return false;
    wchar_t cloudPath[SAVE_BACKUP_PATH_CAP] = {};
    wchar_t parent[SAVE_BACKUP_PATH_CAP] = {};
    const DWORD appDataLength = GetEnvironmentVariableW(
        L"APPDATA", parent, static_cast<DWORD>(_countof(parent)));
    if (!appDataLength || appDataLength >= _countof(parent)) return false;
    wchar_t productRoot[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(productRoot, _countof(productRoot), parent,
                            L"Nippon Ichi Software, Inc\\Honogurashinoniwa") ||
        !SaveBackupJoinPath(cloudPath, _countof(cloudPath), productRoot,
                            L"steam_autocloud.vdf")) return false;
    char content[4096] = {};
    DWORD used = 0;
    if (!SaveBackupReadSmallFile(cloudPath, content, sizeof(content), &used)) {
        return false;
    }
    static constexpr char key[] = "accountid";
    const char* begin = strstr(content, key);
    if (!begin) return false;
    begin += sizeof(key) - 1;
    while (*begin && (*begin < '0' || *begin > '9')) ++begin;
    if (!*begin) return false;
    u64 value = 0;
    unsigned digits = 0;
    while (*begin >= '0' && *begin <= '9') {
        const unsigned digit = static_cast<unsigned>(*begin - '0');
        if (value > (UINT64_MAX - digit) / 10ULL || ++digits > 10) {
            return false;
        }
        value = value * 10ULL + digit;
        ++begin;
    }
    if (digits == 0 || value > 0xffffffffULL) return false;
    *accountIdOut = value;
    return true;
}

static bool SaveBackupDigitsOnly(const wchar_t* text) {
    if (!text || !text[0]) return false;
    const size_t length = wcslen(text);
    if (length < 15 || length > 20) return false;
    for (size_t index = 0; index < length; ++index) {
        if (text[index] < L'0' || text[index] > L'9') return false;
    }
    return true;
}

static bool SaveBackupBuildProductRoot(wchar_t* output, size_t capacity) {
    wchar_t appData[SAVE_BACKUP_PATH_CAP] = {};
    const DWORD length = GetEnvironmentVariableW(
        L"APPDATA", appData, static_cast<DWORD>(_countof(appData)));
    return length != 0 && length < _countof(appData) &&
        SaveBackupJoinPath(output, capacity, appData,
            L"Nippon Ichi Software, Inc\\Honogurashinoniwa");
}

static bool SaveBackupResolveLiveProfile() {
    wchar_t productRoot[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupBuildProductRoot(productRoot, _countof(productRoot)) ||
        !SaveBackupDirectoryIsPlain(productRoot)) return false;

    wchar_t steamId[32] = {};
    u64 accountId = 0;
    if (SaveBackupReadSteamAccountId(&accountId) &&
        accountId <= UINT64_MAX - SAVE_BACKUP_STEAM_ID64_BASE) {
        const u64 steamIdValue = SAVE_BACKUP_STEAM_ID64_BASE + accountId;
        _snwprintf_s(steamId, _countof(steamId), _TRUNCATE,
                     L"%llu", static_cast<unsigned long long>(steamIdValue));
        wchar_t candidate[SAVE_BACKUP_PATH_CAP] = {};
        if (SaveBackupJoinPath(candidate, _countof(candidate), productRoot,
                               steamId) &&
            SaveBackupDirectoryIsPlain(candidate)) {
            wchar_t systemPath[SAVE_BACKUP_PATH_CAP] = {};
            if (SaveBackupJoinPath(systemPath, _countof(systemPath), candidate,
                                   L".systemsave") &&
                GetFileAttributesW(systemPath) != INVALID_FILE_ATTRIBUTES) {
                wcscpy_s(g_saveBackupSteamId, steamId);
                wcscpy_s(g_saveBackupLiveRoot, candidate);
                return true;
            }
        }
    }

    wchar_t pattern[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(pattern, _countof(pattern), productRoot, L"*")) {
        return false;
    }
    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return false;
    ULONGLONG newest = 0;
    wchar_t newestId[32] = {};
    wchar_t newestPath[SAVE_BACKUP_PATH_CAP] = {};
    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
            !SaveBackupDigitsOnly(found.cFileName)) continue;
        wchar_t candidate[SAVE_BACKUP_PATH_CAP] = {};
        wchar_t systemPath[SAVE_BACKUP_PATH_CAP] = {};
        if (!SaveBackupJoinPath(candidate, _countof(candidate), productRoot,
                                found.cFileName) ||
            !SaveBackupJoinPath(systemPath, _countof(systemPath), candidate,
                                L".systemsave")) continue;
        WIN32_FILE_ATTRIBUTE_DATA info = {};
        if (!GetFileAttributesExW(systemPath, GetFileExInfoStandard, &info) ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }
        const ULONGLONG written = SaveBackupFileTimeValue(info.ftLastWriteTime);
        if (!newestPath[0] || written > newest) {
            newest = written;
            wcscpy_s(newestId, found.cFileName);
            wcscpy_s(newestPath, candidate);
        }
    } while (FindNextFileW(search, &found));
    FindClose(search);
    if (!newestPath[0]) return false;
    wcscpy_s(g_saveBackupSteamId, newestId);
    wcscpy_s(g_saveBackupLiveRoot, newestPath);
    return true;
}

static bool SaveBackupPrepareProfileRoot() {
    if (!g_saveBackupSteamId[0] || !g_saveBackupLiveRoot[0] ||
        !SaveBackupJoinPath(g_saveBackupProfileRoot,
                            _countof(g_saveBackupProfileRoot),
                            g_saveBackupRoot, g_saveBackupSteamId) ||
        !SaveBackupEnsureDirectory(g_saveBackupProfileRoot)) return false;
    SaveBackupFileSet initial = {};
    if (!SaveBackupEnumerateLiveFiles(g_saveBackupLiveRoot, &initial)) {
        return false;
    }
    g_saveBackupObserved = initial;
    g_saveBackupDirty = true;
    g_saveBackupStableSince = GetTickCount64();
    g_saveBackupSourceReady = true;
    g_saveBackupSourceMissingLogged = false;
    Log("[SaveBackup] source_ready steam_id=%ls files=%llu bytes=%llu "
        "live_write_access=0 stable_ms=%llu\n",
        g_saveBackupSteamId,
        static_cast<unsigned long long>(initial.count),
        static_cast<unsigned long long>(initial.totalSize),
        static_cast<unsigned long long>(SAVE_BACKUP_STABLE_MS));
    return true;
}

static bool SaveBackupWriteReadme() {
    wchar_t path[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(path, _countof(path), g_saveBackupRoot,
                            L"README_如何恢复存档.txt")) return false;
    static constexpr wchar_t text[] =
        L"Village in the Shade - 自动存档备份 / Automatic Save Backups\r\n"
        L"================================================================\r\n\r\n"
        L"这些文件由 Village QoL Mod 只读复制；Mod 不会改写正在使用的原存档。\r\n"
        L"每个快照都是完整文件组：.systemsave、save.lst 和全部 save.NNN。\r\n"
        L"DAWN 表示游戏显示 06:00-11:59，MIDNIGHT 表示 00:00-05:59，\r\n"
        L"OTHER 表示其余时段；它们标注的是捕获时刻，不是游戏事件名称。\r\n"
        L"仅当磁盘上的完整存档组真实变化且稳定后才会备份；若午夜没有\r\n"
        L"原生写盘，就不会凭空生成 MIDNIGHT 快照。每个游戏日每类只保留\r\n"
        L"最新一份，并只保留\r\n"
        L"最近捕获的三个游戏日。manifest.txt 记录每个文件的 SHA-256；\r\n"
        L"只有其中含 complete=1 的目录可恢复。\r\n\r\n"
        L"恢复步骤（务必完整执行）：\r\n"
        L"1. 正常退出游戏，再退出 Steam；不要在游戏运行时恢复。\r\n"
        L"2. 暂停该游戏的 Steam Cloud，或准备在云冲突时选择本地文件。\r\n"
        L"3. 先手动复制当前存档目录，作为恢复前保险。\r\n"
        L"4. 在本目录选择 SteamID、Day 和快照；确认 manifest.txt 的\r\n"
        L"   complete=1。\r\n"
        L"5. 将快照中的 .systemsave、save.lst、全部 save.NNN 一起复制到：\r\n"
        L"   %APPDATA%\\Nippon Ichi Software, Inc\\Honogurashinoniwa\\<SteamID64>\r\n"
        L"   并覆盖同名文件。不要只恢复 save.NNN；save.lst 含槽位元数据。\r\n"
        L"6. 启动 Steam 和游戏。若 Steam Cloud 冲突，选择刚恢复的本地文件。\r\n\r\n"
        L"English recovery guide:\r\n"
        L"DAWN/MIDNIGHT/OTHER describe the in-game display time at capture. A\r\n"
        L"snapshot is made only after a real, stable on-disk save change; no\r\n"
        L"MIDNIGHT snapshot is invented when the game does not write at midnight.\r\n"
        L"manifest.txt records the SHA-256 of every file.\r\n"
        L"1. Exit the game and Steam completely. Never restore while the game runs.\r\n"
        L"2. Pause Steam Cloud, or choose Local Files if Steam reports a conflict.\r\n"
        L"3. Make a separate copy of the current live save directory first.\r\n"
        L"4. Choose a snapshot whose manifest.txt says complete=1.\r\n"
        L"5. Copy .systemsave, save.lst, and every save.NNN together into:\r\n"
        L"   %APPDATA%\\Nippon Ichi Software, Inc\\Honogurashinoniwa\\<SteamID64>\r\n"
        L"   and overwrite matching files. Do not restore a slot without save.lst.\r\n"
        L"6. Start Steam and the game; choose the restored local copy on conflicts.\r\n";
    return SaveBackupWriteUtf8(path, text);
}

static bool SaveBackupOwnedSnapshotMarker(const wchar_t* directory,
                                          bool allowPartial) {
    if (SaveBackupMarkerEquals(directory, SAVE_BACKUP_FINAL_MARKER_NAME,
                               SAVE_BACKUP_FINAL_MARKER)) return true;
    return allowPartial &&
        SaveBackupMarkerEquals(directory, SAVE_BACKUP_PARTIAL_MARKER_NAME,
                               SAVE_BACKUP_PARTIAL_MARKER);
}

static bool SaveBackupRemoveOwnedSnapshot(const wchar_t* directory,
                                          bool allowPartial) {
    if (!SaveBackupPathIsUnderProfile(directory) ||
        !SaveBackupDirectoryIsPlain(directory) ||
        !SaveBackupOwnedSnapshotMarker(directory, allowPartial)) return false;
    wchar_t pattern[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(pattern, _countof(pattern), directory, L"*")) {
        return false;
    }
    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return false;
    bool owned = true;
    do {
        if (_wcsicmp(found.cFileName, L".") == 0 ||
            _wcsicmp(found.cFileName, L"..") == 0) continue;
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
            !SaveBackupSnapshotEntryIsOwned(found.cFileName)) {
            owned = false;
            break;
        }
    } while (FindNextFileW(search, &found));
    FindClose(search);
    if (!owned) return false;

    search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return false;
    bool deleted = true;
    do {
        if (_wcsicmp(found.cFileName, L".") == 0 ||
            _wcsicmp(found.cFileName, L"..") == 0) continue;
        wchar_t path[SAVE_BACKUP_PATH_CAP] = {};
        if (!SaveBackupJoinPath(path, _countof(path), directory,
                                found.cFileName) || !DeleteFileW(path)) {
            deleted = false;
        }
    } while (FindNextFileW(search, &found));
    FindClose(search);
    return deleted && RemoveDirectoryW(directory) != FALSE;
}

static bool SaveBackupParseDayName(const wchar_t* name, u64* rawDayOut) {
    // Generated directories are exactly Day_ plus ten decimal digits.  The
    // strict length also keeps every retention copy inside its fixed buffer;
    // unknown user-created names are ignored instead of reaching wcscpy_s.
    if (!name || wcslen(name) != 14 || wcsncmp(name, L"Day_", 4) != 0) {
        return false;
    }
    u64 displayedDay = 0;
    for (size_t index = 4; index < 14; ++index) {
        if (name[index] < L'0' || name[index] > L'9') return false;
        displayedDay = displayedDay * 10ULL +
            static_cast<u64>(name[index] - L'0');
    }
    if (displayedDay == 0) return false;
    if (rawDayOut) *rawDayOut = displayedDay - 1;
    return true;
}

static bool SaveBackupDayMarkerValid(const wchar_t* directory) {
    if (!directory) return false;
    const wchar_t* name = wcsrchr(directory, L'\\');
    if (!name) name = wcsrchr(directory, L'/');
    name = name ? name + 1 : directory;
    u64 rawDay = 0;
    if (!SaveBackupParseDayName(name, &rawDay)) return false;
    char expected[96] = {};
    const int length = _snprintf_s(
        expected, sizeof(expected), _TRUNCATE, "%s%llu\n",
        SAVE_BACKUP_DAY_MARKER_PREFIX,
        static_cast<unsigned long long>(rawDay));
    return length > 0 &&
        SaveBackupMarkerEquals(directory, SAVE_BACKUP_DAY_MARKER_NAME,
                               expected, false);
}

static bool SaveBackupRemoveOwnedDay(const wchar_t* dayDirectory) {
    if (!SaveBackupPathIsUnderProfile(dayDirectory) ||
        !SaveBackupDirectoryIsPlain(dayDirectory) ||
        !SaveBackupDayMarkerValid(dayDirectory)) return false;
    wchar_t pattern[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(pattern, _countof(pattern), dayDirectory, L"*")) {
        return false;
    }
    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return false;
    bool owned = true;
    do {
        if (_wcsicmp(found.cFileName, L".") == 0 ||
            _wcsicmp(found.cFileName, L"..") == 0) continue;
        if (_wcsicmp(found.cFileName, SAVE_BACKUP_DAY_MARKER_NAME) == 0 &&
            (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
            (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
            continue;
        }
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            owned = false;
            break;
        }
        wchar_t child[SAVE_BACKUP_PATH_CAP] = {};
        if (!SaveBackupJoinPath(child, _countof(child), dayDirectory,
                                found.cFileName) ||
            !SaveBackupOwnedSnapshotMarker(child, true)) {
            owned = false;
            break;
        }
    } while (FindNextFileW(search, &found));
    FindClose(search);
    if (!owned) return false;

    search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return false;
    bool deleted = true;
    do {
        if (_wcsicmp(found.cFileName, L".") == 0 ||
            _wcsicmp(found.cFileName, L"..") == 0 ||
            _wcsicmp(found.cFileName, SAVE_BACKUP_DAY_MARKER_NAME) == 0) {
            continue;
        }
        wchar_t child[SAVE_BACKUP_PATH_CAP] = {};
        if (!SaveBackupJoinPath(child, _countof(child), dayDirectory,
                                found.cFileName) ||
            !SaveBackupRemoveOwnedSnapshot(child, true)) deleted = false;
    } while (FindNextFileW(search, &found));
    FindClose(search);
    if (!deleted) return false;
    wchar_t marker[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(marker, _countof(marker), dayDirectory,
                            SAVE_BACKUP_DAY_MARKER_NAME) ||
        !DeleteFileW(marker)) return false;
    return RemoveDirectoryW(dayDirectory) != FALSE;
}

static bool SaveBackupWriteDayMarker(const wchar_t* dayDirectory, u64 dayId) {
    wchar_t path[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(path, _countof(path), dayDirectory,
                            SAVE_BACKUP_DAY_MARKER_NAME)) return false;
    char marker[96] = {};
    const int length = _snprintf_s(
        marker, sizeof(marker), _TRUNCATE, "%s%llu\n",
        SAVE_BACKUP_DAY_MARKER_PREFIX,
        static_cast<unsigned long long>(dayId));
    return length > 0 && SaveBackupWriteBytes(
        path, marker, static_cast<DWORD>(length));
}

static void SaveBackupPruneCategory(const wchar_t* dayDirectory,
                                    SaveBackupCategory category,
                                    const wchar_t* keepName) {
    wchar_t pattern[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(pattern, _countof(pattern), dayDirectory, L"*")) {
        return;
    }
    const wchar_t* categoryName = SaveBackupCategoryName(category);
    const size_t categoryLength = wcslen(categoryName);
    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }
        wchar_t child[SAVE_BACKUP_PATH_CAP] = {};
        if (!SaveBackupJoinPath(child, _countof(child), dayDirectory,
                                found.cFileName)) continue;
        if (_wcsnicmp(found.cFileName, L".partial_", 9) == 0) {
            if (!SaveBackupRemoveOwnedSnapshot(child, true)) {
                Log("[SaveBackup] cleanup_skip kind=partial path=%ls "
                    "reason=UNOWNED_OR_BUSY\n", child);
            }
            continue;
        }
        if (_wcsicmp(found.cFileName, keepName) == 0 ||
            _wcsnicmp(found.cFileName, categoryName, categoryLength) != 0 ||
            found.cFileName[categoryLength] != L'_') continue;
        if (!SaveBackupRemoveOwnedSnapshot(child, false)) {
            Log("[SaveBackup] cleanup_skip kind=category path=%ls "
                "reason=UNOWNED_OR_BUSY\n", child);
        }
    } while (FindNextFileW(search, &found));
    FindClose(search);
}

static void SaveBackupPruneDays() {
    if (!SaveBackupDirectoryIsPlain(g_saveBackupProfileRoot)) return;
    wchar_t pattern[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(pattern, _countof(pattern),
                            g_saveBackupProfileRoot, L"*")) return;
    SaveBackupDayCandidate candidates[SAVE_BACKUP_MAX_DAYS] = {};
    size_t count = 0;
    bool overflow = false;
    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
            (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
            !SaveBackupParseDayName(found.cFileName, nullptr)) continue;
        if (count >= SAVE_BACKUP_MAX_DAYS) {
            overflow = true;
            break;
        }
        wchar_t dayPath[SAVE_BACKUP_PATH_CAP] = {};
        wchar_t markerPath[SAVE_BACKUP_PATH_CAP] = {};
        if (!SaveBackupJoinPath(dayPath, _countof(dayPath),
                                g_saveBackupProfileRoot, found.cFileName) ||
            !SaveBackupDayMarkerValid(dayPath) ||
            !SaveBackupJoinPath(markerPath, _countof(markerPath), dayPath,
                                SAVE_BACKUP_DAY_MARKER_NAME)) continue;
        WIN32_FILE_ATTRIBUTE_DATA markerInfo = {};
        if (!GetFileAttributesExW(markerPath, GetFileExInfoStandard,
                                  &markerInfo) ||
            (markerInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            (markerInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }
        if (wcscpy_s(candidates[count].name,
                     _countof(candidates[count].name),
                     found.cFileName) != 0) {
            continue;
        }
        candidates[count].touched =
            SaveBackupFileTimeValue(markerInfo.ftLastWriteTime);
        ++count;
    } while (FindNextFileW(search, &found));
    FindClose(search);
    if (overflow) {
        Log("[SaveBackup] retention_skip reason=TOO_MANY_DAY_DIRECTORIES "
            "limit=%llu\n",
            static_cast<unsigned long long>(SAVE_BACKUP_MAX_DAYS));
        return;
    }
    for (size_t left = 0; left < count; ++left) {
        for (size_t right = left + 1; right < count; ++right) {
            if (candidates[right].touched > candidates[left].touched) {
                const SaveBackupDayCandidate temporary = candidates[left];
                candidates[left] = candidates[right];
                candidates[right] = temporary;
            }
        }
    }
    for (size_t index = 3; index < count; ++index) {
        wchar_t path[SAVE_BACKUP_PATH_CAP] = {};
        if (!SaveBackupJoinPath(path, _countof(path),
                                g_saveBackupProfileRoot,
                                candidates[index].name)) continue;
        if (SaveBackupRemoveOwnedDay(path)) {
            Log("[SaveBackup] retention_removed day=%ls policy=recent_3\n",
                candidates[index].name);
        } else {
            Log("[SaveBackup] retention_skip day=%ls "
                "reason=UNOWNED_OR_BUSY\n", candidates[index].name);
        }
    }
}

static bool SaveBackupCopyOne(const wchar_t* sourceDirectory,
                              const wchar_t* targetDirectory,
                              const SaveBackupFileIdentity& identity) {
    wchar_t source[SAVE_BACKUP_PATH_CAP] = {};
    wchar_t target[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(source, _countof(source), sourceDirectory,
                            identity.name) ||
        !SaveBackupJoinPath(target, _countof(target), targetDirectory,
                            identity.name)) return false;
    const DWORD sourceAttributes = GetFileAttributesW(source);
    if (sourceAttributes == INVALID_FILE_ATTRIBUTES ||
        (sourceAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (sourceAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) return false;

    // Never deny the game's writer or rename/delete path.  Use an explicit
    // shared read handle instead of an opaque convenience-copy source handle.
    HANDLE sourceFile = CreateFileW(
        source, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (sourceFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sourceSize = {};
    bool copied = GetFileSizeEx(sourceFile, &sourceSize) &&
        sourceSize.QuadPart >= 0 &&
        static_cast<ULONGLONG>(sourceSize.QuadPart) == identity.size;
    HANDLE targetFile = INVALID_HANDLE_VALUE;
    if (copied) {
        targetFile = CreateFileW(
            target, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
            nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        copied = targetFile != INVALID_HANDLE_VALUE;
    }
    unsigned char buffer[64 * 1024] = {};
    ULONGLONG remaining = identity.size;
    while (copied && remaining != 0) {
        const DWORD requested = static_cast<DWORD>(
            remaining < sizeof(buffer) ? remaining : sizeof(buffer));
        DWORD read = 0;
        DWORD written = 0;
        copied = ReadFile(sourceFile, buffer, requested, &read, nullptr) &&
                 read == requested &&
                 WriteFile(targetFile, buffer, read, &written, nullptr) &&
                 written == read;
        if (copied) remaining -= read;
    }
    SecureZeroMemory(buffer, sizeof(buffer));
    if (copied) copied = FlushFileBuffers(targetFile) != FALSE;
    if (targetFile != INVALID_HANDLE_VALUE) CloseHandle(targetFile);
    CloseHandle(sourceFile);
    if (!copied) return false;

    WIN32_FILE_ATTRIBUTE_DATA targetInfo = {};
    if (!GetFileAttributesExW(target, GetFileExInfoStandard, &targetInfo) ||
        (targetInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
        (targetInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return false;
    }
    ULARGE_INTEGER size = {};
    size.LowPart = targetInfo.nFileSizeLow;
    size.HighPart = targetInfo.nFileSizeHigh;
    if (size.QuadPart != identity.size ||
        !SaveBackupReadContainerMagic(target)) return false;
    return true;
}

static bool SaveBackupWriteManifest(
        const wchar_t* partialDirectory,
        const SaveBackupFileSet& files,
        const SaveBackupHashSet& hashes,
        const SaveBackupClockSnapshot& clock,
        SaveBackupCategory category,
        const SYSTEMTIME& capturedAt) {
    if (hashes.count != files.count) return false;
    wchar_t path[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(path, _countof(path), partialDirectory,
                            L"manifest.txt")) return false;
    char manifest[32768] = {};
    int used = _snprintf_s(
        manifest, sizeof(manifest), _TRUNCATE,
        "format=Village_QoL_Save_Backup_V1\r\n"
        "complete=1\r\n"
        "steam_id=%ls\r\n"
        "category=%s\r\n"
        "game_day=%llu\r\n"
        "raw_day_id=%llu\r\n"
        "raw_second=%lld\r\n"
        "game_time=%02u:%02u:%02u\r\n"
        "world_epoch=%llu\r\n"
        "load_generation=%llu\r\n"
        "captured_local=%04u-%02u-%02uT%02u:%02u:%02u\r\n"
        "file_count=%llu\r\n"
        "total_bytes=%llu\r\n",
        g_saveBackupSteamId, SaveBackupCategoryNameAscii(category),
        static_cast<unsigned long long>(clock.dayId + 1),
        static_cast<unsigned long long>(clock.dayId),
        static_cast<long long>(clock.rawSecond), clock.hour, clock.minute,
        clock.second,
        static_cast<unsigned long long>(clock.worldEpoch),
        static_cast<unsigned long long>(clock.loadGeneration),
        capturedAt.wYear, capturedAt.wMonth, capturedAt.wDay,
        capturedAt.wHour, capturedAt.wMinute, capturedAt.wSecond,
        static_cast<unsigned long long>(files.count),
        static_cast<unsigned long long>(files.totalSize));
    if (used <= 0) return false;
    for (size_t index = 0; index < files.count; ++index) {
        char sha256[65] = {};
        SaveBackupSha256Text(hashes.files[index].sha256, sha256);
        const int added = _snprintf_s(
            manifest + used, sizeof(manifest) - static_cast<size_t>(used),
            _TRUNCATE,
            "file=%ls size=%llu sha256=%s last_write_100ns=%llu\r\n",
            files.files[index].name,
            static_cast<unsigned long long>(files.files[index].size),
            sha256,
            static_cast<unsigned long long>(files.files[index].lastWrite));
        if (added <= 0) return false;
        used += added;
    }
    return SaveBackupWriteBytes(path, manifest, static_cast<DWORD>(used));
}

static SaveBackupCaptureResult SaveBackupCapture(
        const SaveBackupFileSet& expected,
        const SaveBackupClockSnapshot& clock) {
    if (!SaveBackupDirectoryIsPlain(g_saveBackupLiveRoot) ||
        !SaveBackupDirectoryIsPlain(g_saveBackupProfileRoot)) {
        return SaveBackupCaptureResult::Failed;
    }
    SaveBackupFileSet before = {};
    if (!SaveBackupEnumerateLiveFiles(g_saveBackupLiveRoot, &before)) {
        return SaveBackupCaptureResult::Failed;
    }
    if (!SaveBackupFileSetEqual(before, expected)) {
        g_saveBackupObserved = before;
        g_saveBackupStableSince = GetTickCount64();
        return SaveBackupCaptureResult::SourceChanged;
    }

    // Content pass A happens before any copy.  PackageHashFile opens its
    // read-only handle with FILE_SHARE_READ | FILE_SHARE_WRITE |
    // FILE_SHARE_DELETE, so this cannot block the game's native save path.
    SaveBackupHashSet sourcePassA = {};
    if (!SaveBackupHashFileSet(g_saveBackupLiveRoot, before, &sourcePassA)) {
        g_saveBackupStableSince = GetTickCount64();
        return SaveBackupCaptureResult::SourceChanged;
    }

    if (clock.dayId >= 9999999999ULL) {
        SetLastError(ERROR_ARITHMETIC_OVERFLOW);
        return SaveBackupCaptureResult::Failed;
    }
    const SaveBackupCategory category = SaveBackupClassify(clock);
    wchar_t dayName[48] = {};
    _snwprintf_s(dayName, _countof(dayName), _TRUNCATE, L"Day_%010llu",
                 static_cast<unsigned long long>(clock.dayId + 1));
    wchar_t dayDirectory[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(dayDirectory, _countof(dayDirectory),
                            g_saveBackupProfileRoot, dayName) ||
        !SaveBackupEnsureDirectory(dayDirectory)) {
        return SaveBackupCaptureResult::Failed;
    }

    const DWORD processId = GetCurrentProcessId();
    wchar_t partialName[80] = {};
    wchar_t partialDirectory[SAVE_BACKUP_PATH_CAP] = {};
    bool partialCreated = false;
    for (unsigned attempt = 0; attempt < 16 && !partialCreated; ++attempt) {
        const unsigned sequence = ++g_saveBackupSequence;
        _snwprintf_s(partialName, _countof(partialName), _TRUNCATE,
                     L".partial_%08lX_%08X",
                     static_cast<unsigned long>(processId), sequence);
        if (!SaveBackupJoinPath(partialDirectory,
                                _countof(partialDirectory),
                                dayDirectory, partialName)) return
            SaveBackupCaptureResult::Failed;
        if (CreateDirectoryW(partialDirectory, nullptr)) {
            partialCreated = SaveBackupDirectoryIsPlain(partialDirectory);
        } else if (GetLastError() != ERROR_ALREADY_EXISTS) {
            break;
        }
    }
    if (!partialCreated || !SaveBackupPathIsUnderProfile(partialDirectory)) {
        return SaveBackupCaptureResult::Failed;
    }
    wchar_t partialMarker[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(partialMarker, _countof(partialMarker),
                            partialDirectory,
                            SAVE_BACKUP_PARTIAL_MARKER_NAME) ||
        !SaveBackupWriteBytes(
            partialMarker, SAVE_BACKUP_PARTIAL_MARKER,
            static_cast<DWORD>(sizeof(SAVE_BACKUP_PARTIAL_MARKER) - 1))) {
        RemoveDirectoryW(partialDirectory);
        return SaveBackupCaptureResult::Failed;
    }

    bool copied = true;
    for (size_t index = 0; index < before.count; ++index) {
        if (!SaveBackupCopyOne(g_saveBackupLiveRoot, partialDirectory,
                               before.files[index])) {
            copied = false;
            break;
        }
    }

    SaveBackupHashSet targetHashes = {};
    const bool targetHashesReadable = copied &&
        SaveBackupHashFileSet(partialDirectory, before, &targetHashes);
    SaveBackupHashSet sourcePassB = {};
    const bool sourcePassBReadable =
        SaveBackupHashFileSet(g_saveBackupLiveRoot, before, &sourcePassB);
    SaveBackupFileSet after = {};
    const bool afterReadable =
        SaveBackupEnumerateLiveFiles(g_saveBackupLiveRoot, &after);
    const bool directoryUnchanged =
        afterReadable && SaveBackupFileSetEqual(before, after);
    const bool sourceContentUnchanged = sourcePassBReadable &&
        SaveBackupHashSetEqual(sourcePassA, sourcePassB);
    const bool targetMatchesSource = targetHashesReadable &&
        SaveBackupHashSetEqual(sourcePassA, targetHashes);
    if (!copied || !directoryUnchanged || !sourceContentUnchanged ||
        !targetMatchesSource) {
        SaveBackupRemoveOwnedSnapshot(partialDirectory, true);
        if (!directoryUnchanged || !sourceContentUnchanged) {
            if (afterReadable) g_saveBackupObserved = after;
            g_saveBackupStableSince = GetTickCount64();
            return SaveBackupCaptureResult::SourceChanged;
        }
        return SaveBackupCaptureResult::Failed;
    }

    SYSTEMTIME capturedAt = {};
    GetLocalTime(&capturedAt);
    if (!SaveBackupWriteManifest(partialDirectory, before, sourcePassA,
                                 clock, category, capturedAt)) {
        SaveBackupRemoveOwnedSnapshot(partialDirectory, true);
        return SaveBackupCaptureResult::Failed;
    }
    wchar_t finalMarker[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(finalMarker, _countof(finalMarker),
                            partialDirectory,
                            SAVE_BACKUP_FINAL_MARKER_NAME) ||
        !SaveBackupWriteBytes(
            finalMarker, SAVE_BACKUP_FINAL_MARKER,
            static_cast<DWORD>(sizeof(SAVE_BACKUP_FINAL_MARKER) - 1)) ||
        !DeleteFileW(partialMarker)) {
        SaveBackupRemoveOwnedSnapshot(partialDirectory, true);
        return SaveBackupCaptureResult::Failed;
    }

    wchar_t finalName[96] = {};
    _snwprintf_s(
        finalName, _countof(finalName), _TRUNCATE,
        L"%ls_%04u%02u%02u_%02u%02u%02u_%04X",
        SaveBackupCategoryName(category), capturedAt.wYear, capturedAt.wMonth,
        capturedAt.wDay, capturedAt.wHour, capturedAt.wMinute,
        capturedAt.wSecond, g_saveBackupSequence & 0xffffu);
    wchar_t finalDirectory[SAVE_BACKUP_PATH_CAP] = {};
    if (!SaveBackupJoinPath(finalDirectory, _countof(finalDirectory),
                            dayDirectory, finalName) ||
        !MoveFileExW(partialDirectory, finalDirectory,
                     MOVEFILE_WRITE_THROUGH)) {
        SaveBackupRemoveOwnedSnapshot(partialDirectory, true);
        return SaveBackupCaptureResult::Failed;
    }

    // Touch the day ledger only after the complete snapshot has been
    // published.  Retention therefore tracks the three most recently captured
    // logical days even when loading an older save moves the numeric day back.
    if (!SaveBackupWriteDayMarker(dayDirectory, clock.dayId)) {
        Log("[SaveBackup] day_marker_refresh_failed path=%ls\n", dayDirectory);
    }
    SaveBackupPruneCategory(dayDirectory, category, finalName);
    SaveBackupPruneDays();
    Log("[SaveBackup] snapshot_published steam_id=%ls game_day=%llu "
        "raw_day_id=%llu game_time=%02u:%02u:%02u category=%s files=%llu "
        "bytes=%llu path=%ls source_unchanged=1 sha256_a_target_b=1 "
        "complete=1\n",
        g_saveBackupSteamId,
        static_cast<unsigned long long>(clock.dayId + 1),
        static_cast<unsigned long long>(clock.dayId), clock.hour,
        clock.minute, clock.second, SaveBackupCategoryNameAscii(category),
        static_cast<unsigned long long>(before.count),
        static_cast<unsigned long long>(before.totalSize), finalDirectory);
    return SaveBackupCaptureResult::Published;
}

static bool InitializeSaveBackup() {
    if (g_saveBackupInitialized) return true;
    if (!PackageWritesAuthorized() || !g_module) return false;
    wchar_t modulePath[SAVE_BACKUP_PATH_CAP] = {};
    const DWORD length = GetModuleFileNameW(
        g_module, modulePath, static_cast<DWORD>(_countof(modulePath)));
    if (!length || length >= _countof(modulePath) - 1) return false;
    wchar_t* separator = wcsrchr(modulePath, L'\\');
    if (!separator) separator = wcsrchr(modulePath, L'/');
    if (!separator || separator == modulePath) return false;
    *separator = L'\0';
    if (!SaveBackupJoinPath(g_saveBackupRoot, _countof(g_saveBackupRoot),
                            modulePath, SAVE_BACKUP_DIRECTORY_NAME) ||
        !SaveBackupEnsureDirectory(g_saveBackupRoot) ||
        !SaveBackupWriteReadme()) {
        Log("[SaveBackup] initialize_failed stage=root_or_readme "
            "win32=%lu\n", static_cast<unsigned long>(GetLastError()));
        return false;
    }
    g_saveBackupInitialized = true;
    g_saveBackupNextPollAt = GetTickCount64();
    g_saveBackupNextAccountRetryAt = g_saveBackupNextPollAt;
    if (SaveBackupResolveLiveProfile() && SaveBackupPrepareProfileRoot()) {
        Log("[SaveBackup] initialized root=%ls retention_game_days=3 "
            "categories=DAWN,MIDNIGHT,OTHER live_save_writes=0\n",
            g_saveBackupRoot);
    } else {
        g_saveBackupLiveRoot[0] = L'\0';
        g_saveBackupSteamId[0] = L'\0';
        g_saveBackupProfileRoot[0] = L'\0';
        g_saveBackupSourceReady = false;
        Log("[SaveBackup] initialized root=%ls source_pending=1 "
            "live_save_writes=0\n", g_saveBackupRoot);
    }
    return true;
}

static void PumpSaveBackup() {
    if (!g_saveBackupInitialized || !PackageWritesAuthorized()) return;
    const ULONGLONG now = GetTickCount64();
    if (now < g_saveBackupNextPollAt) return;
    g_saveBackupNextPollAt = now + SAVE_BACKUP_POLL_MS;

    if (!g_saveBackupSourceReady) {
        if (now < g_saveBackupNextAccountRetryAt) return;
        g_saveBackupNextAccountRetryAt = now + SAVE_BACKUP_ACCOUNT_RETRY_MS;
        if (!SaveBackupResolveLiveProfile() || !SaveBackupPrepareProfileRoot()) {
            if (!g_saveBackupSourceMissingLogged) {
                g_saveBackupSourceMissingLogged = true;
                Log("[SaveBackup] source_waiting appdata_profile_unavailable=1\n");
            }
            g_saveBackupLiveRoot[0] = L'\0';
            g_saveBackupSteamId[0] = L'\0';
            g_saveBackupProfileRoot[0] = L'\0';
            return;
        }
    }

    SaveBackupFileSet current = {};
    if (!SaveBackupEnumerateLiveFiles(g_saveBackupLiveRoot, &current)) {
        if (!g_saveBackupSourceMissingLogged) {
            g_saveBackupSourceMissingLogged = true;
            Log("[SaveBackup] source_unstable_or_unavailable retry=1\n");
        }
        g_saveBackupDirty = true;
        g_saveBackupStableSince = now;
        return;
    }
    g_saveBackupSourceMissingLogged = false;
    if (!SaveBackupFileSetEqual(current, g_saveBackupObserved)) {
        g_saveBackupObserved = current;
        g_saveBackupDirty = true;
        g_saveBackupStableSince = now;
        Log("[SaveBackup] live_change_observed files=%llu bytes=%llu "
            "debounce_ms=%llu\n",
            static_cast<unsigned long long>(current.count),
            static_cast<unsigned long long>(current.totalSize),
            static_cast<unsigned long long>(SAVE_BACKUP_STABLE_MS));
        return;
    }
    if (!g_saveBackupDirty || now - g_saveBackupStableSince <
                                  SAVE_BACKUP_STABLE_MS) return;

    SaveBackupClockSnapshot clock = {};
    if (!SaveBackupReadClock(&clock)) return;
    const SaveBackupCaptureResult result =
        SaveBackupCapture(g_saveBackupObserved, clock);
    if (result == SaveBackupCaptureResult::Published) {
        g_saveBackupDirty = false;
    } else if (result == SaveBackupCaptureResult::SourceChanged) {
        g_saveBackupDirty = true;
    } else {
        g_saveBackupNextPollAt = GetTickCount64() + SAVE_BACKUP_RETRY_MS;
        Log("[SaveBackup] snapshot_retry reason=COPY_OR_VALIDATION_FAILED "
            "win32=%lu\n", static_cast<unsigned long>(GetLastError()));
    }
}
