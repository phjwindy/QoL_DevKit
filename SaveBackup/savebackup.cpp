// savebackup.cpp —— SaveBackup: 自动存档备份
//
// 从 dinput8.cpp 原始 .inl 提取的独立 DLL 插件。
// 实现逻辑见 save_backup.inl（原始文件，无需 RVA 迁移）。
//
// 本文件提供：
//   1. .inl 所需的全部外部依赖桩函数
//   2. ReadGameRawSecond — 直接读取游戏时钟全局指针，供分类备份
//
// .inl 在文件末尾通过 #include 引入。

#include <windows.h>
#include <wincrypt.h>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <new>

// ============================================================
// 类型别名（dinput8.cpp 中的定义）
// ============================================================
using u64 = std::uint64_t;
using u32 = std::uint32_t;

// ============================================================
// 日志（QoL_Shared）
// ============================================================
#include "logging.h"

// SaveBackup 日志开关：调试版启用日志
#define SAVEBACKUP_LOGGING
#ifdef SAVEBACKUP_LOGGING
  // 使用 QoL_Shared 的日志系统
#else
  #define LogOpen(x)  ((void)0)
  #define Log(...)    ((void)0)
  #define LogV(...)   ((void)0)
  #define LogClose()  ((void)0)
#endif

// ============================================================
// g_supportedExe — 新 build 指纹验证
// ============================================================
static bool g_supportedExe = false;

// ============================================================
// g_module — .inl 通过 g_module 引用
// ============================================================
static HMODULE g_module = nullptr;

// ============================================================
// IsReadable — 内存可读检查（与 dinput8.cpp 一致）
// ============================================================
static bool IsReadable(const void* pointer, size_t size) {
    if (!pointer || size == 0) return false;
    uintptr_t start = reinterpret_cast<uintptr_t>(pointer);
    if (start < 0x10000 || start > 0x00007FFFFFFFFFFFULL) return false;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(pointer, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
    uintptr_t regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    uintptr_t regionEnd = regionStart + mbi.RegionSize;
    if (start + size < start || start + size > regionEnd) return false;
    return true;
}

// ============================================================
// PackageWritesAuthorized — 独立 DLL 无包验证，直接允许
// ============================================================
static bool PackageWritesAuthorized() {
    return true;
}

static void PackageOpenWriteGate() {
    // 独立 DLL：无需包验证
}


// ============================================================
// PackageHashFile — SHA-256 文件哈希（从 author_integrity.inl 提取）
// ============================================================
static bool PackageHashFile(const wchar_t* path, unsigned char output[32],
                            unsigned* fileSize, DWORD* error) {
    if (error) *error = ERROR_SUCCESS;
    HANDLE file = CreateFileW(path, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (error) *error = GetLastError();
        return false;
    }
    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(file, &size)) {
        if (error) *error = GetLastError();
        CloseHandle(file);
        return false;
    }
    if (size.QuadPart < 0 ||
        static_cast<unsigned long long>(size.QuadPart) > 0xffffffffULL) {
        if (error) *error = ERROR_FILE_TOO_LARGE;
        CloseHandle(file);
        return false;
    }

    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    bool ok = CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES,
                                   CRYPT_VERIFYCONTEXT) != FALSE;
    if (ok) ok = CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) != FALSE;
    if (!ok && error) *error = GetLastError();
    unsigned char buffer[64 * 1024] = {};
    while (ok) {
        DWORD read = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) {
            if (error) *error = GetLastError();
            ok = false;
            break;
        }
        if (!read) break;
        if (!CryptHashData(hash, buffer, read, 0)) {
            if (error) *error = GetLastError();
            ok = false;
        }
    }
    DWORD hashSize = 32;
    if (ok) {
        ok = CryptGetHashParam(hash, HP_HASHVAL, output, &hashSize, 0) != FALSE &&
             hashSize == 32;
        if (!ok && error) *error = GetLastError();
    }
    SecureZeroMemory(buffer, sizeof(buffer));
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    if (ok && fileSize) *fileSize = static_cast<unsigned>(size.QuadPart);
    return ok;
}

// ============================================================
// build 指纹验证
// ============================================================
#include <bcrypt.h>

static bool VerifyExeBuild() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    HANDLE hFile = CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        Log("[SaveBackup] VerifyExeBuild: CreateFileW failed (err=%lu path=%ls)\n",
            GetLastError(), exePath);
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart != 18134536) {
        CloseHandle(hFile);
        return false;
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        CloseHandle(hFile);
        return false;
    }
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        return false;
    }

    unsigned char buf[65536];
    DWORD bytesRead;
    while (ReadFile(hFile, buf, sizeof(buf), &bytesRead, nullptr) && bytesRead > 0) {
        BCryptHashData(hHash, buf, bytesRead, 0);
    }
    CloseHandle(hFile);

    unsigned char hash[32];
    BCryptFinishHash(hHash, hash, sizeof(hash), 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    // build 25094764 SHA-256
    static const unsigned char expected[32] = {
        0x7D, 0xC5, 0xAD, 0x61, 0x45, 0x41, 0xFB, 0x77,
        0x08, 0xE4, 0x20, 0x36, 0xDF, 0x7B, 0x7D, 0xA8,
        0x50, 0x53, 0xAB, 0x49, 0x59, 0xF6, 0xED, 0x2E,
        0x9D, 0xFE, 0x13, 0x30, 0xAE, 0xDB, 0xD3, 0x81
    };

    bool match = memcmp(hash, expected, sizeof(hash)) == 0;
    if (match) {
        Log("[SaveBackup] village.exe verified: build 25094764 (1.09)\n");
    } else {
        Log("[SaveBackup] village.exe SHA-256 mismatch; feature disabled\n");
    }
    return match;
}


// ============================================================
// ReadGameRawSecond — 直接读取游戏时钟，替代旧版 CGameTime::advance hook
// 全局指针 [base+0x10D4950] -> GameTimeState, currentRawSecond @ +0x08
// ============================================================
static constexpr uintptr_t RVA_GAME_TIME_GLOBAL = 0x10D4950;

static std::int64_t ReadGameRawSecond() {
    const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!base) return -1;
    const void* globalAddr = reinterpret_cast<const void*>(
        base + RVA_GAME_TIME_GLOBAL);
    if (!IsReadable(globalAddr, sizeof(uintptr_t))) return -1;
    const uintptr_t statePtr =
        *reinterpret_cast<const uintptr_t*>(globalAddr);
    if (!IsReadable(reinterpret_cast<const void*>(statePtr),
                     sizeof(std::int64_t))) return -1;
    return *reinterpret_cast<const std::int64_t*>(statePtr + 0x08);
}

// ============================================================
// 前向声明（实现在 save_backup.inl 中）
// ============================================================
static bool InitializeSaveBackup();
static void PumpSaveBackup();

// ============================================================
// 插件入口
// ============================================================

extern "C" __declspec(dllexport) void mod_init(void) {
    LogOpen("savebackup");
    Log("[SaveBackup] mod_init — build 25094764 v1.09\n");

    g_module = GetModuleHandleW(nullptr);

    g_supportedExe = VerifyExeBuild();
    if (!g_supportedExe) {
        Log("[SaveBackup] unsupported exe; feature disabled safely\n");
        return;
    }

    PackageOpenWriteGate();

    // 初始化存档备份
    const bool backupReady = InitializeSaveBackup();
    Log("[SaveBackup] startup_ready=%d\n", backupReady ? 1 : 0);
}

extern "C" __declspec(dllexport) void mod_tick(void) {
    // 每帧调用 PumpSaveBackup 执行轮询和备份
    PumpSaveBackup();
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID /*lpReserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = hMod;
        DisableThreadLibraryCalls(hMod);
    }
    return TRUE;
}

// ============================================================
// 引入 .inl 实现（所有外部依赖已在上方提供）
// ============================================================
#include "save_backup.inl"
