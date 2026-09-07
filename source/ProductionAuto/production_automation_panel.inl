// ---- production automation side panel and atomic mod-owned configuration --
#pragma once

#include <xinput.h>

static constexpr wchar_t PRODUCTION_PANEL_CLASS[] =
    L"VillageQoLProductionAutomation18";
static constexpr wchar_t PRODUCTION_CONFIG_FILE[] =
    L"Village_QoL_Production_Automation.json";
static constexpr int PRODUCTION_PANEL_BASE_WIDTH = 520;
static constexpr int PRODUCTION_PANEL_BASE_HEIGHT = 650;
static constexpr int PRODUCTION_PANEL_ROWS = 12;
static constexpr size_t PRODUCTION_CONFIG_MAX_BYTES = 262144;
static constexpr ULONGLONG
    PRODUCTION_PANEL_WORLD_CALLBACK_STALE_MS = 2500;
static constexpr ULONGLONG
    PRODUCTION_PANEL_WORLD_CALLBACK_HARD_RELEASE_MS = 10000;
static_assert(PRODUCTION_PANEL_WORLD_CALLBACK_HARD_RELEASE_MS >
              PRODUCTION_PANEL_WORLD_CALLBACK_STALE_MS);

static HWND g_productionPanelWindow = nullptr;
static std::atomic<HWND> g_productionPanelPublishedWindow{nullptr};
static std::atomic<HWND> g_productionPanelPublishedOwner{nullptr};
static HWND g_productionPanelOwner = nullptr;
static HFONT g_productionPanelTitleFont = nullptr;
static HFONT g_productionPanelBodyFont = nullptr;
static HFONT g_productionPanelSmallFont = nullptr;
static int g_productionPanelWidth = PRODUCTION_PANEL_BASE_WIDTH;
static int g_productionPanelHeight = PRODUCTION_PANEL_BASE_HEIGHT;
static int g_productionPanelTab = 0;
static size_t g_productionPanelScroll = 0;
static size_t g_productionPanelSelection = 0;
static bool g_productionConfigLoaded = false;
static wchar_t g_productionConfigPath[MAX_PATH] = {};
static bool g_productionKeyboardArmed = true;
static bool g_productionControllerArmed = true;
static bool g_productionPanelWorldTimedOut = false;
static bool g_productionPanelInFlightSlowLogged = false;
static bool g_productionPanelHardReleaseLogged = false;
static u64 g_productionPanelInputGeneration = 0;
static int g_productionPanelPressedTarget = 0;
static size_t g_productionPanelPressedRow = 0;
static u64 g_productionPanelPressedGeneration = 0;

// ---- floating machine-status marker window (mod-owned overlay) ----------
// A layered, click-through window parked above the game window while a
// machine is locked.  It repeats the verified active-machine identity and
// the mod-owned binding/automation state in Chinese with state color coding.
// It owns no game object, pointer, or prompt: every refresh re-reads the
// value-only published snapshot and re-resolves the active identity.
static constexpr wchar_t PRODUCTION_MARKER_CLASS[] =
    L"VillageQoLProductionMarker18";
static HWND g_productionMarkerWindow = nullptr;
static HFONT g_productionMarkerFont = nullptr;
static wchar_t g_productionMarkerText[256] = {};
static COLORREF g_productionMarkerAccent = RGB(80, 205, 118);
// Only true after one successful panel visual confirmation for the current
// lock; cleared on lifecycle/timeout/master-toggle teardown.
static bool g_productionMarkerAllowed = false;
static bool g_productionMarkerWasVisible = false;
static wchar_t g_productionMarkerLastText[256] = {};
static COLORREF g_productionMarkerLastAccent = RGB(0, 0, 0);

using ProductionXInputGetStateFunction = DWORD (WINAPI *)(DWORD, XINPUT_STATE*);
static ProductionXInputGetStateFunction g_productionXInputGetState = nullptr;
static HMODULE g_productionXInputModule = nullptr;
static const volatile unsigned char g_productionXInputGetStateEncoded[15] = {
    0xfd, 0xec, 0xcb, 0xd5, 0xd0, 0xd1, 0xe2, 0xc0,
    0xd1, 0xf6, 0xd1, 0xc4, 0xd1, 0xc0, 0xa5,
};

static int ProductionScale(int value) {
    UINT dpi = 96;
    using GetDpiForWindowFunction = UINT (WINAPI *)(HWND);
    static GetDpiForWindowFunction getDpiForWindow =
        reinterpret_cast<GetDpiForWindowFunction>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (getDpiForWindow && g_productionPanelOwner)
        dpi = getDpiForWindow(g_productionPanelOwner);
    return MulDiv(value, static_cast<int>(dpi ? dpi : 96), 96);
}

static bool ProductionGetConfigPath() {
    if (g_productionConfigPath[0]) return true;
    wchar_t modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(g_module, modulePath, MAX_PATH);
    if (!length || length >= MAX_PATH - 1) return false;
    wchar_t* slash = wcsrchr(modulePath, L'\\');
    if (!slash) return false;
    slash[1] = L'\0';
    if (wcslen(modulePath) + wcslen(PRODUCTION_CONFIG_FILE) >= MAX_PATH)
        return false;
    wcscpy_s(g_productionConfigPath, modulePath);
    wcscat_s(g_productionConfigPath, PRODUCTION_CONFIG_FILE);
    return true;
}

static bool ProductionWriteAll(HANDLE file, const void* data, DWORD size) {
    const unsigned char* cursor = static_cast<const unsigned char*>(data);
    DWORD remaining = size;
    while (remaining) {
        DWORD written = 0;
        if (!WriteFile(file, cursor, remaining, &written, nullptr) || !written)
            return false;
        cursor += written;
        remaining -= written;
    }
    return true;
}

static bool ProductionSaveConfig() {
    if (!ProductionGetConfigPath()) return false;
    ProductionBinding snapshot[PRODUCTION_MAX_BINDINGS] = {};
    size_t count = 0;
    AcquireSRWLockShared(&g_productionBindingLock);
    for (size_t index = 0; index < g_productionBindingCount &&
                           count < PRODUCTION_MAX_BINDINGS; ++index) {
        if (!g_productionBindings[index].committed) continue;
        snapshot[count++] = g_productionBindings[index];
    }
    ReleaseSRWLockShared(&g_productionBindingLock);

    char* content = static_cast<char*>(HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, PRODUCTION_CONFIG_MAX_BYTES));
    if (!content) return false;
    size_t used = 0;
    int added = _snprintf_s(
        content, PRODUCTION_CONFIG_MAX_BYTES, _TRUNCATE,
        "{\n  \"schema_version\":1,\n  \"release\":\"1.8\",\n  \"bindings\":[\n");
    bool ok = added > 0;
    if (ok) used = static_cast<size_t>(added);
    for (size_t index = 0; ok && index < count; ++index) {
        const ProductionBinding& value = snapshot[index];
        added = _snprintf_s(
            content + used, PRODUCTION_CONFIG_MAX_BYTES - used, _TRUNCATE,
            "    {\"machine_map\":%llu,\"machine_id\":%llu,"
            "\"input_map\":%llu,\"input_id\":%llu,"
            "\"output_map\":%llu,\"output_id\":%llu,"
            "\"has_input\":%u,\"has_output\":%u,\"enabled\":%u}%s\n",
            static_cast<unsigned long long>(value.machine.mapId),
            static_cast<unsigned long long>(value.machine.uniqueId),
            static_cast<unsigned long long>(value.input.mapId),
            static_cast<unsigned long long>(value.input.uniqueId),
            static_cast<unsigned long long>(value.output.mapId),
            static_cast<unsigned long long>(value.output.uniqueId),
            value.hasInput ? 1u : 0u, value.hasOutput ? 1u : 0u,
            value.enabled ? 1u : 0u, index + 1 == count ? "" : ",");
        if (added <= 0 || static_cast<size_t>(added) >=
                              PRODUCTION_CONFIG_MAX_BYTES - used) {
            ok = false;
            break;
        }
        used += static_cast<size_t>(added);
    }
    if (ok) {
        added = _snprintf_s(content + used, PRODUCTION_CONFIG_MAX_BYTES - used,
                            _TRUNCATE, "  ]\n}\n");
        ok = added > 0;
        if (ok) used += static_cast<size_t>(added);
    }

    wchar_t temporary[MAX_PATH] = {};
    if (ok) {
        _snwprintf_s(temporary, MAX_PATH, _TRUNCATE, L"%ls.tmp.%lu.%llu",
                     g_productionConfigPath,
                     static_cast<unsigned long>(GetCurrentProcessId()),
                     static_cast<unsigned long long>(
                         g_productionTxnSequence.fetch_add(
                             1, std::memory_order_relaxed) + 1));
        HANDLE file = CreateFileW(
            temporary, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_WRITE_THROUGH, nullptr);
        ok = file != INVALID_HANDLE_VALUE;
        if (ok) {
            ok = ProductionWriteAll(file, content, static_cast<DWORD>(used)) &&
                 FlushFileBuffers(file) != FALSE;
            if (!CloseHandle(file)) ok = false;
        }
    }
    HeapFree(GetProcessHeap(), 0, content);
    if (!ok) {
        if (temporary[0]) DeleteFileW(temporary);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=config_write result=FAIL "
            "reason=atomic_temp_write win32=%lu\n", GetLastError());
        return false;
    }

    const DWORD attributes = GetFileAttributesW(g_productionConfigPath);
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        ok = ReplaceFileW(g_productionConfigPath, temporary, nullptr,
                          REPLACEFILE_WRITE_THROUGH, nullptr, nullptr) != FALSE;
    } else {
        ok = MoveFileExW(temporary, g_productionConfigPath,
                         MOVEFILE_WRITE_THROUGH) != FALSE;
    }
    if (!ok) DeleteFileW(temporary);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=config_write result=%s reason=%s "
        "schema=1 bindings=%zu native_save_writes=0\n",
        ok ? "PASS" : "FAIL", ok ? "none" : "atomic_replace", count);
    return ok;
}

static void ProductionQuarantineCorruptConfig(const char* reason) {
    if (!ProductionGetConfigPath()) return;
    SYSTEMTIME time = {};
    GetSystemTime(&time);
    wchar_t corrupt[MAX_PATH] = {};
    _snwprintf_s(corrupt, MAX_PATH, _TRUNCATE,
                 L"%ls.corrupt.%04u%02u%02uT%02u%02u%02uZ",
                 g_productionConfigPath,
                 static_cast<unsigned int>(time.wYear),
                 static_cast<unsigned int>(time.wMonth),
                 static_cast<unsigned int>(time.wDay),
                 static_cast<unsigned int>(time.wHour),
                 static_cast<unsigned int>(time.wMinute),
                 static_cast<unsigned int>(time.wSecond));
    const bool moved = MoveFileExW(g_productionConfigPath, corrupt,
                                   MOVEFILE_WRITE_THROUGH) != FALSE;
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=config_recovery result=%s reason=%s "
        "corrupt_quarantined=%d\n", moved ? "PASS" : "FAIL",
        reason ? reason : "parse", moved ? 1 : 0);
}

static void ProductionEnsureActiveBindingLocked() {
    const ProductionStableId active = ProductionGetActiveMachine();
    if (active.mapId && active.uniqueId)
        ProductionFindBindingLocked(active, true);
}

static bool ProductionLoadConfig() {
    if (g_productionConfigLoaded) return true;
    g_productionConfigLoaded = true;
    if (!ProductionGetConfigPath()) return false;
    HANDLE file = CreateFileW(g_productionConfigPath, GENERIC_READ,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            AcquireSRWLockExclusive(&g_productionBindingLock);
            ProductionEnsureActiveBindingLocked();
            ReleaseSRWLockExclusive(&g_productionBindingLock);
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=config_load result=PASS "
                "reason=new_config schema=1 bindings=0\n");
            return true;
        }
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=config_load result=FAIL "
            "reason=file_open win32=%lu\n", error);
        return false;
    }
    LARGE_INTEGER size = {};
    bool ok = GetFileSizeEx(file, &size) != FALSE && size.QuadPart > 0 &&
              size.QuadPart < static_cast<LONGLONG>(PRODUCTION_CONFIG_MAX_BYTES);
    char* content = nullptr;
    if (ok) {
        content = static_cast<char*>(HeapAlloc(
            GetProcessHeap(), HEAP_ZERO_MEMORY,
            static_cast<SIZE_T>(size.QuadPart) + 1));
        ok = content != nullptr;
    }
    DWORD read = 0;
    if (ok) {
        ok = ReadFile(file, content, static_cast<DWORD>(size.QuadPart), &read,
                      nullptr) != FALSE && read == size.QuadPart;
    }
    CloseHandle(file);
    ProductionBinding parsed[PRODUCTION_MAX_BINDINGS] = {};
    size_t parsedCount = 0;
    if (ok) {
        ok = strstr(content, "\"schema_version\":1") != nullptr &&
             strstr(content, "\"release\":\"1.8\"") != nullptr &&
             strstr(content, "\"bindings\":[") != nullptr;
    }
    const char* cursor = ok ? content : nullptr;
    while (ok && (cursor = strstr(cursor, "{\"machine_map\":")) != nullptr) {
        if (parsedCount >= PRODUCTION_MAX_BINDINGS) {
            ok = false;
            break;
        }
        unsigned long long machineMap = 0, machineId = 0;
        unsigned long long inputMap = 0, inputId = 0;
        unsigned long long outputMap = 0, outputId = 0;
        unsigned hasInput = 0, hasOutput = 0, enabled = 0;
        int consumed = 0;
        const int fields = sscanf(
            cursor,
            "{\"machine_map\":%llu,\"machine_id\":%llu,"
            "\"input_map\":%llu,\"input_id\":%llu,"
            "\"output_map\":%llu,\"output_id\":%llu,"
            "\"has_input\":%u,\"has_output\":%u,\"enabled\":%u}%n",
            &machineMap, &machineId, &inputMap, &inputId, &outputMap,
            &outputId, &hasInput, &hasOutput, &enabled, &consumed);
        if (fields != 9 || consumed <= 0 || !machineMap || !machineId ||
            hasInput > 1 || hasOutput > 1 || enabled > 1 ||
            (hasInput && (!inputMap || !inputId)) ||
            (hasOutput && (!outputMap || !outputId))) {
            ok = false;
            break;
        }
        ProductionBinding value = {};
        value.machine = {static_cast<u64>(machineMap),
                         static_cast<u64>(machineId)};
        value.input = {static_cast<u64>(inputMap), static_cast<u64>(inputId)};
        value.output = {static_cast<u64>(outputMap), static_cast<u64>(outputId)};
        value.hasInput = hasInput != 0;
        value.hasOutput = hasOutput != 0;
        value.enabled = enabled != 0;
        value.committed = true;
        value.state = ProductionState::Unbound;
        value.reason = ProductionReason::InputUnbound;
        for (size_t prior = 0; prior < parsedCount; ++prior) {
            if (ProductionIdEqual(parsed[prior].machine, value.machine)) {
                ok = false;
                break;
            }
        }
        if (!ok) break;
        parsed[parsedCount++] = value;
        cursor += consumed;
    }
    if (content) HeapFree(GetProcessHeap(), 0, content);
    if (!ok) {
        ProductionQuarantineCorruptConfig("schema_or_binding_parse");
        AcquireSRWLockExclusive(&g_productionBindingLock);
        memset(g_productionBindings, 0, sizeof(g_productionBindings));
        g_productionBindingCount = 0;
        ProductionEnsureActiveBindingLocked();
        ReleaseSRWLockExclusive(&g_productionBindingLock);
        return false;
    }
    AcquireSRWLockExclusive(&g_productionBindingLock);
    memcpy(g_productionBindings, parsed, parsedCount * sizeof(parsed[0]));
    g_productionBindingCount = parsedCount;
    // A UI hook can publish the active stable ID before this worker-thread
    // load completes.  Preserve a usable active binding after replacement.
    ProductionEnsureActiveBindingLocked();
    ReleaseSRWLockExclusive(&g_productionBindingLock);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=config_load result=PASS reason=none "
        "schema=1 bindings=%zu native_save_writes=0\n", parsedCount);
    ProductionPrimeAllEnabledBindingsDirty("config_load");
    return true;
}

static void ProductionUtf8ToWide(const char* input, wchar_t* output,
                                 size_t outputCount) {
    if (!output || outputCount == 0) return;
    output[0] = L'\0';
    if (!input || !input[0]) return;
    const int converted = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, input, -1, output,
        static_cast<int>(outputCount));
    if (converted <= 0) wcscpy_s(output, outputCount, L"<invalid UTF-8>");
}

static void ProductionSanitizeLogName(const char* input, char* output,
                                      size_t outputCount) {
    ProductionCopyLogToken(input, output, outputCount);
}

static bool ProductionGetActiveBinding(ProductionBinding* output) {
    if (!output) return false;
    const ProductionStableId active = ProductionGetActiveMachine();
    if (!active.mapId || !active.uniqueId) return false;
    bool found = false;
    AcquireSRWLockShared(&g_productionBindingLock);
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        if (!ProductionIdEqual(g_productionBindings[index].machine, active))
            continue;
        *output = g_productionBindings[index];
        found = true;
        break;
    }
    ReleaseSRWLockShared(&g_productionBindingLock);
    return found;
}

static void ProductionGetPublishedBindingNames(
        const ProductionBinding& binding,
        char* inputName, size_t inputNameCount,
        char* outputName, size_t outputNameCount) {
    strcpy_s(inputName, inputNameCount,
             binding.hasInput ? "unresolved" : "unbound");
    strcpy_s(outputName, outputNameCount,
             binding.hasOutput ? "unresolved" : "unbound");
    AcquireSRWLockShared(&g_productionPublishLock);
    for (size_t index = 0; index < g_productionPublished.chestCount; ++index) {
        const ProductionObjectSnapshot& chest =
            g_productionPublished.chests[index];
        if (binding.hasInput && ProductionIdEqual(binding.input, chest.id))
            ProductionSanitizeLogName(chest.name, inputName, inputNameCount);
        if (binding.hasOutput && ProductionIdEqual(binding.output, chest.id))
            ProductionSanitizeLogName(chest.name, outputName, outputNameCount);
    }
    ReleaseSRWLockShared(&g_productionPublishLock);
}

static void ProductionRefreshBindingMutationState(
        ProductionBinding* binding, ProductionReason rejected) {
    if (!binding) return;
    if (rejected != ProductionReason::None) {
        binding->state = ProductionState::Faulted;
        binding->reason = rejected;
    } else if (!binding->enabled) {
        binding->state = ProductionState::Idle;
        binding->reason = ProductionReason::PausedByUser;
    } else if (!binding->hasInput) {
        binding->state = ProductionState::Unbound;
        binding->reason = ProductionReason::InputUnbound;
    } else if (!binding->hasOutput) {
        binding->state = ProductionState::Unbound;
        binding->reason = ProductionReason::OutputUnbound;
    } else {
        binding->state = PRODUCTION_NATIVE_TRANSACTIONS_PROVEN
                             ? ProductionState::Starting
                             : ProductionState::Faulted;
        binding->reason = PRODUCTION_NATIVE_TRANSACTIONS_PROVEN
                              ? ProductionReason::BindingSettlePending
                              : ProductionReason::NativeTransactionEvidenceRequired;
    }
}

static void ProductionRefreshPublishedBindingState(
        const ProductionStableId& active,
        const ProductionBinding* binding) {
    AcquireSRWLockExclusive(&g_productionPublishLock);
    if (g_productionPublished.activeMachinePresent &&
        ProductionIdEqual(g_productionPublished.activeMachine, active)) {
        g_productionPublished.activeState = binding
            ? binding->state : ProductionState::Unbound;
        g_productionPublished.activeReason = binding
            ? binding->reason : ProductionReason::InputUnbound;
    }
    ReleaseSRWLockExclusive(&g_productionPublishLock);
}

static bool ProductionPublishedMutationGuardLocked(
        const ProductionStableId& active, u64 requiredGeneration) {
    if (!ProductionFeatureIsEnabled() || !requiredGeneration ||
        !g_productionReady.load(std::memory_order_acquire) ||
        g_productionFaulted.load(std::memory_order_acquire) ||
        g_productionRegistryScanRequested.load(std::memory_order_acquire) ||
        g_productionScanInProgress.load(std::memory_order_acquire) ||
        g_productionPublished.scanActive ||
        g_productionPublished.generation != requiredGeneration ||
        g_productionSequence.load(std::memory_order_acquire) !=
            requiredGeneration ||
        !g_productionPublished.activeMachinePresent ||
        !ProductionIdEqual(g_productionPublished.activeMachine, active)) {
        return false;
    }
    return true;
}

static bool ProductionPublishedChestMutationGuardLocked(
        const ProductionStableId& active, const ProductionStableId& chest,
        u64 requiredGeneration) {
    if (!ProductionPublishedMutationGuardLocked(active, requiredGeneration))
        return false;
    size_t matches = 0;
    for (size_t index = 0; index < g_productionPublished.chestCount; ++index) {
        const ProductionObjectSnapshot& candidate =
            g_productionPublished.chests[index];
        if (ProductionIdEqual(candidate.id, chest) && candidate.chest &&
            candidate.named && candidate.inventoryValid) {
            ++matches;
        }
    }
    return matches == 1;
}

static bool ProductionMutateBinding(int action, const ProductionStableId* chest,
                                    const char* displayName,
                                    u64 requiredGeneration = 0) {
    if (action < 0 || action > 4 ||
        ((action == 0 || action == 1) && !chest)) return false;

    AcquireSRWLockShared(&g_productionPublishLock);
    // Non-list actions (unbind/toggle) capture the generation while the same
    // shared lock prevents scan completion or invalidation from racing their
    // binding commit.  Chest selection supplies the generation copied with
    // the selected stable ID and is revalidated below.
    if (!requiredGeneration)
        requiredGeneration = g_productionPublished.generation;

    bool changed = false;
    bool bindingFound = false;
    ProductionReason rejected = ProductionReason::None;
    ProductionBinding before = {};
    ProductionBinding after = {};
    AcquireSRWLockExclusive(&g_productionBindingLock);
    // The main thread publishes this reservation while it revalidates and
    // commits one native output.  Reject (rather than queue) a mouse mutation
    // during that tiny window so a copied candidate can never deliver to an
    // output chest that the panel has just replaced or unbound.
    const bool transferInFlight =
        g_productionTransferInFlight.load(std::memory_order_acquire);
    const bool mutationAlreadyPending =
        g_productionBindingMutationInProgress.load(
            std::memory_order_acquire);
    const bool panelOpenCommitPending =
        g_productionPanelOpenCommitInProgress.load(
            std::memory_order_acquire);
    if (transferInFlight || mutationAlreadyPending || panelOpenCommitPending) {
        const u64 publishedGenerationAtGuard =
            g_productionPublished.generation;
        ReleaseSRWLockExclusive(&g_productionBindingLock);
        ReleaseSRWLockShared(&g_productionPublishLock);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=binding_guard action=%d "
            "result=INCOMPLETE reason=%s "
            "required_generation=%llu published_generation=%llu "
            "binding_changed=0 native_input_consumed=0\n",
            action, transferInFlight ? "transfer_in_flight" :
                    (mutationAlreadyPending ? "binding_mutation_pending" :
                                              "panel_open_commit_pending"),
            static_cast<unsigned long long>(requiredGeneration),
            static_cast<unsigned long long>(publishedGenerationAtGuard));
        return false;
    }
    ProductionStableId active = {};
    AcquireSRWLockShared(&g_productionActiveLock);
    active = g_productionActiveMachine;
    const bool activeValid = active.mapId && active.uniqueId &&
        !g_productionFaulted.load(std::memory_order_acquire);
    const u64 publishedGenerationAtGuard = g_productionPublished.generation;
    const bool scanActiveAtGuard = g_productionPublished.scanActive ||
        g_productionRegistryScanRequested.load(std::memory_order_acquire) ||
        g_productionScanInProgress.load(std::memory_order_acquire);
    const bool publishedValid = chest
        ? ProductionPublishedChestMutationGuardLocked(
              active, *chest, requiredGeneration)
        : ProductionPublishedMutationGuardLocked(active, requiredGeneration);
    if (!activeValid || !publishedValid) {
        ReleaseSRWLockShared(&g_productionActiveLock);
        ReleaseSRWLockExclusive(&g_productionBindingLock);
        ReleaseSRWLockShared(&g_productionPublishLock);
        if (g_productionPanelWindow)
            InvalidateRect(g_productionPanelWindow, nullptr, FALSE);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=binding_guard action=%d "
            "result=FAIL reason=mutation_guard_rejected "
            "device_map_id=%llu device_id=%llu required_generation=%llu "
            "published_generation=%llu scan_active=%d active_valid=%d "
            "binding_changed=0 native_input_consumed=0\n",
            action, static_cast<unsigned long long>(active.mapId),
            static_cast<unsigned long long>(active.uniqueId),
            static_cast<unsigned long long>(requiredGeneration),
            static_cast<unsigned long long>(publishedGenerationAtGuard),
            scanActiveAtGuard ? 1 : 0, activeValid ? 1 : 0);
        return false;
    }
    ProductionBinding* binding = ProductionFindBindingLocked(active, false);
    if (binding && binding->committed) {
        bindingFound = true;
        before = *binding;
        if (action == 0 && chest) {
            if (binding->hasOutput && ProductionIdEqual(binding->output, *chest))
                rejected = ProductionReason::SameChestRejected;
            else {
                binding->input = *chest;
                binding->hasInput = true;
                changed = true;
            }
        } else if (action == 1 && chest) {
            if (binding->hasInput && ProductionIdEqual(binding->input, *chest))
                rejected = ProductionReason::SameChestRejected;
            else {
                binding->output = *chest;
                binding->hasOutput = true;
                changed = true;
            }
        } else if (action == 2) {
            binding->input = {};
            binding->hasInput = false;
            changed = true;
        } else if (action == 3) {
            binding->output = {};
            binding->hasOutput = false;
            changed = true;
        } else if (action == 4) {
            binding->enabled = !binding->enabled;
            changed = true;
        }
        if (changed || rejected != ProductionReason::None)
            ProductionRefreshBindingMutationState(binding, rejected);
        if (changed) {
            g_productionBindingMutationInProgress.store(
                true, std::memory_order_release);
            g_productionBindingRevision.fetch_add(1,
                                                   std::memory_order_acq_rel);
            ProductionArmBindingMutationTransferGate();
        }
        after = *binding;
    }
    ReleaseSRWLockShared(&g_productionActiveLock);
    ReleaseSRWLockExclusive(&g_productionBindingLock);
    ReleaseSRWLockShared(&g_productionPublishLock);

    const bool configSaved = !changed || ProductionSaveConfig();
    bool committed = changed && configSaved;
    bool rollbackRestored = false;
    bool finalBindingPresent = bindingFound;
    if (changed) {
        AcquireSRWLockExclusive(&g_productionBindingLock);
        if (!configSaved) {
            ProductionBinding* current =
                ProductionFindBindingLocked(active, false);
            if (current) {
                *current = before;
                after = before;
                rollbackRestored = true;
                g_productionBindingRevision.fetch_add(
                    1, std::memory_order_acq_rel);
            }
        }
        // The durable commit/rollback edge is the safety epoch.  Re-arm here
        // so time spent in file I/O cannot satisfy the settle/callback gate.
        ProductionArmBindingMutationTransferGate();
        g_productionBindingMutationInProgress.store(
            false, std::memory_order_release);
        ReleaseSRWLockExclusive(&g_productionBindingLock);
        if (!configSaved && !rollbackRestored)
            g_productionFaulted.store(true, std::memory_order_release);
    }

    const bool refreshRequired = changed || rejected != ProductionReason::None;
    // No full registry rescan after a binding mutation: the binding revision
    // marks this device dirty for the game-time due state machine, while the
    // panel repaints immediately from the already-updated value table.
    bool scanRequested = false;
    if (refreshRequired) {
        ProductionRefreshPublishedBindingState(
            active, finalBindingPresent ? &after : nullptr);
        if (g_productionPanelWindow)
            InvalidateRect(g_productionPanelWindow, nullptr, FALSE);
    }

    char safeName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    ProductionSanitizeLogName(displayName, safeName, sizeof(safeName));
    char inputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    char outputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    ProductionGetPublishedBindingNames(after, inputName, sizeof(inputName),
                                       outputName, sizeof(outputName));
    if (committed && action == 0 && safeName[0])
        strcpy_s(inputName, sizeof(inputName), safeName);
    if (committed && action == 1 && safeName[0])
        strcpy_s(outputName, sizeof(outputName), safeName);
    const u64 txn =
        g_productionTxnSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool safeRejected =
        !changed && rejected == ProductionReason::SameChestRejected;
    const char* result = committed ? "INCOMPLETE" :
                         safeRejected ? "PASS" : "FAIL";
    const ProductionReason logReason =
        changed && !committed ? ProductionReason::ConfigCorrupt
        : (changed || rejected != ProductionReason::None) ? after.reason
        : ProductionReason::RegistryInvalid;
    const char* rollback = changed && !committed
        ? (rollbackRestored ? "restored" : "failed") : "not_needed";
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=%llu event=binding action=%d "
        "device_map_id=%llu device_id=%llu input_map_id=%llu input_id=%llu "
        "output_map_id=%llu output_id=%llu input_name=%s output_name=%s "
        "state=%s from=%s to=%s recipe=0 planned=0 removed=0 written=0 "
        "rollback=%s result=%s reason=%s case=%s scan_requested=%d "
        "settle_gate=1 post_edge_callback_gate=1 mutation_pending=0\n",
        static_cast<unsigned long long>(txn),
        action, static_cast<unsigned long long>(active.mapId),
        static_cast<unsigned long long>(active.uniqueId),
        static_cast<unsigned long long>(after.input.mapId),
        static_cast<unsigned long long>(after.input.uniqueId),
        static_cast<unsigned long long>(after.output.mapId),
        static_cast<unsigned long long>(after.output.uniqueId),
        inputName, outputName, ProductionStateName(after.state),
        ProductionStateName(before.state),
        ProductionStateName(after.state),
        rollback, bindingFound ? result : "FAIL", ProductionReasonName(logReason),
        action == 0 ? "bind_input" : action == 1 ? "bind_output" :
        action == 2 ? "unbind_input" : action == 3 ? "unbind_output" :
                      "toggle_enabled",
        scanRequested ? 1 : 0);
    return committed;
}

static void ProductionDrawText(HDC dc, HFONT font, COLORREF color,
                               const wchar_t* text, RECT rectangle,
                               UINT flags) {
    HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, font));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, text, -1, &rectangle, flags | DT_NOPREFIX);
    SelectObject(dc, oldFont);
}

static LRESULT CALLBACK ProductionMarkerWndProc(HWND window, UINT message,
                                                 WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        HDC dc = BeginPaint(window, &paint);
        RECT client = {};
        GetClientRect(window, &client);
        static HBRUSH s_markerBg = nullptr;
        if (!s_markerBg) s_markerBg = CreateSolidBrush(RGB(22, 25, 30));
        FillRect(dc, &client, s_markerBg);
        RECT accent = client;
        accent.right = accent.left + ProductionScale(10);
        static HBRUSH s_markerAccent = nullptr;
        static COLORREF s_markerAccentColor = 0;
        if (!s_markerAccent || s_markerAccentColor != g_productionMarkerAccent) {
            if (s_markerAccent) DeleteObject(s_markerAccent);
            s_markerAccent = CreateSolidBrush(g_productionMarkerAccent);
            s_markerAccentColor = g_productionMarkerAccent;
        }
        FillRect(dc, &accent, s_markerAccent);
        RECT textRect = client;
        textRect.left += ProductionScale(28);
        textRect.right -= ProductionScale(16);
        HFONT oldFont = reinterpret_cast<HFONT>(
            SelectObject(dc, g_productionMarkerFont));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(246, 247, 249));
        DrawTextW(dc, g_productionMarkerText, -1, &textRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
                      DT_END_ELLIPSIS);
        SelectObject(dc, oldFont);
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

static bool ProductionInitMarkerWindow() {
    if (g_productionMarkerWindow) return true;
    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = ProductionMarkerWndProc;
    cls.hInstance = g_module;
    cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    cls.lpszClassName = PRODUCTION_MARKER_CLASS;
    if (!RegisterClassExW(&cls)) {
        WNDCLASSEXW existing = {};
        existing.cbSize = sizeof(existing);
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS ||
            !GetClassInfoExW(g_module, PRODUCTION_MARKER_CLASS, &existing) ||
            existing.lpfnWndProc != ProductionMarkerWndProc ||
            existing.hInstance != g_module) {
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=marker_init result=FAIL "
                "reason=window_class win32=%lu\n", GetLastError());
            return false;
        }
    }
    g_productionMarkerFont = CreateFontW(
        -ProductionScale(19), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Microsoft YaHei UI");
    if (!g_productionMarkerFont) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=marker_init result=FAIL "
            "reason=font win32=%lu\n", GetLastError());
        return false;
    }
    g_productionMarkerWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED |
            WS_EX_TRANSPARENT,
        PRODUCTION_MARKER_CLASS, L"", WS_POPUP, 0, 0,
        ProductionScale(760), ProductionScale(62),
        nullptr, nullptr, g_module, nullptr);
    if (!g_productionMarkerWindow) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=marker_init result=FAIL "
            "reason=create_window win32=%lu\n", GetLastError());
        return false;
    }
    SetLayeredWindowAttributes(g_productionMarkerWindow, 0, 246, LWA_ALPHA);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=marker_init result=PASS "
        "reason=none kind=mod_owned_floating_chip\n");
    return true;
}

static void ProductionHideMarker() {
    if (g_productionMarkerWindow && IsWindowVisible(g_productionMarkerWindow)) {
        ShowWindow(g_productionMarkerWindow, SW_HIDE);
        if (g_productionMarkerWasVisible) {
            g_productionMarkerWasVisible = false;
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=marker_lifecycle "
                "result=INCOMPLETE reason=lock_context_lost "
                "marker_visible=0 kind=mod_owned_floating_chip\n");
        }
    }
}

static void ProductionMarkerClearAllowed() {
    g_productionMarkerAllowed = false;
    ProductionHideMarker();
}

static void ProductionRefreshMarker() {
    const ProductionStableId active = ProductionGetActiveMachine();
    const bool activePresent = active.mapId != 0 && active.uniqueId != 0;
    if (!g_productionMarkerAllowed || !activePresent) {
        ProductionHideMarker();
        return;
    }
    ProductionBinding binding = {};
    const bool hasBinding = ProductionGetActiveBinding(&binding);
    if (!hasBinding) {
        ProductionHideMarker();
        return;
    }
    char inputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    char outputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    ProductionGetPublishedBindingNames(binding, inputName, sizeof(inputName),
                                       outputName, sizeof(outputName));
    wchar_t inputWide[16] = {};
    wchar_t outputWide[16] = {};
    ProductionUtf8ToWide(inputName, inputWide, 16);
    ProductionUtf8ToWide(outputName, outputWide, 16);
    const wchar_t* inputText = binding.hasInput
        ? (strcmp(inputName, "unresolved") == 0 ? L"解析中…" : inputWide)
        : L"未绑定";
    const wchar_t* outputText = binding.hasOutput
        ? (strcmp(outputName, "unresolved") == 0 ? L"解析中…" : outputWide)
        : L"未绑定";
    // Re-resolve identity and state fresh on every refresh; never cache the
    // binding object or published names across worker iterations.
    const wchar_t* faultSuffix =
        g_productionTransferFaulted.load(std::memory_order_acquire)
            ? L" ｜ ⚠转运已停止"
            : L"";
    _snwprintf_s(
        g_productionMarkerText,
        sizeof(g_productionMarkerText) / sizeof(g_productionMarkerText[0]),
        _TRUNCATE,
        L"已锁定 #%06llX ｜ 状态:%ls ｜ 输入箱:%ls ｜ 输出箱:%ls ｜ 自动化:%ls%ls",
        static_cast<unsigned long long>(active.uniqueId & 0xffffff),
        ProductionStateNameZh(binding.state),
        inputText, outputText,
        binding.enabled ? L"开" : L"关", faultSuffix);
    g_productionMarkerAccent =
        ProductionStateAccent(binding.state, binding.enabled);
    if (!ProductionInitMarkerWindow()) return;

    HWND owner = g_productionPanelOwner ? g_productionPanelOwner
                                        : FindSettingsOwner();
    RECT anchor = {};
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    if (!owner || !GetWindowRect(owner, &anchor)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchor, 0);
    }
    HMONITOR handle = MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(handle, &monitor)) monitor.rcWork = anchor;
    const int width = ProductionScale(760);
    const int height = ProductionScale(62);
    // Left column, directly above the production panel: the status sits at
    // the top of the left page with the panel below it.
    const int panelX = anchor.left - g_productionPanelWidth - ProductionScale(8);
    int x = panelX + (g_productionPanelWidth - width) / 2;
    int y = anchor.top + ProductionScale(16) - height - ProductionScale(6);
    if (y < monitor.rcWork.top) y = anchor.top + ProductionScale(16);
    if (x < monitor.rcWork.left) x = monitor.rcWork.left;
    if (x + width > monitor.rcWork.right) x = monitor.rcWork.right - width;
    if (!SetWindowPos(g_productionMarkerWindow, HWND_TOPMOST, x, y, width,
                      height, SWP_NOACTIVATE | SWP_SHOWWINDOW)) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=marker_lifecycle result=FAIL "
            "reason=set_window_pos marker_visible=0 "
            "kind=mod_owned_floating_chip\n");
        return;
    }
    const bool contentChanged =
        wcscmp(g_productionMarkerLastText, g_productionMarkerText) != 0 ||
        g_productionMarkerLastAccent != g_productionMarkerAccent;
    if (contentChanged) {
        wcscpy_s(g_productionMarkerLastText, g_productionMarkerText);
        g_productionMarkerLastAccent = g_productionMarkerAccent;
        InvalidateRect(g_productionMarkerWindow, nullptr, TRUE);
    }
    if (!g_productionMarkerWasVisible) {
        g_productionMarkerWasVisible = true;
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=marker_lifecycle "
            "result=INCOMPLETE reason=lock_context_active marker_visible=1 "
            "kind=mod_owned_floating_chip device_map_id=%llu "
            "device_id=%llu state=%s enabled=%d\n",
            static_cast<unsigned long long>(active.mapId),
            static_cast<unsigned long long>(active.uniqueId),
            ProductionStateName(binding.state), binding.enabled ? 1 : 0);
    }
}

static size_t ProductionClampPanelSelection() {
    AcquireSRWLockShared(&g_productionPublishLock);
    const size_t chestCount = g_productionPublished.chestCount;
    ReleaseSRWLockShared(&g_productionPublishLock);
    if (chestCount == 0) {
        g_productionPanelSelection = 0;
        g_productionPanelScroll = 0;
        return 0;
    }
    if (g_productionPanelSelection >= chestCount)
        g_productionPanelSelection = chestCount - 1;
    const size_t maximumScroll = chestCount > PRODUCTION_PANEL_ROWS
        ? chestCount - PRODUCTION_PANEL_ROWS : 0;
    if (g_productionPanelScroll > maximumScroll)
        g_productionPanelScroll = maximumScroll;
    if (g_productionPanelSelection < g_productionPanelScroll)
        g_productionPanelScroll = g_productionPanelSelection;
    if (g_productionPanelSelection >=
        g_productionPanelScroll + PRODUCTION_PANEL_ROWS) {
        g_productionPanelScroll =
            g_productionPanelSelection - PRODUCTION_PANEL_ROWS + 1;
    }
    return chestCount;
}

static void ProductionPaintPanel(HWND window) {
    ProductionClampPanelSelection();
    PAINTSTRUCT paint = {};
    HDC dc = BeginPaint(window, &paint);
    RECT client = {};
    GetClientRect(window, &client);
    // Cached static brushes for fixed colors (avoid per-frame GDI alloc/free).
    static HBRUSH s_bgBrush = nullptr;
    static HBRUSH s_closeBrush = nullptr;
    static HBRUSH s_selectedBrush = nullptr;
    static HBRUSH s_normalBrush = nullptr;
    static HBRUSH s_buttonBrush = nullptr;
    static HBRUSH s_rowBoundBrush = nullptr;
    static HBRUSH s_rowSelectedBrush = nullptr;
    static HBRUSH s_rowDefaultBrush = nullptr;
    if (!s_bgBrush) {
        s_bgBrush = CreateSolidBrush(RGB(24, 27, 32));
        s_closeBrush = CreateSolidBrush(RGB(76, 52, 55));
        s_selectedBrush = CreateSolidBrush(RGB(55, 112, 82));
        s_normalBrush = CreateSolidBrush(RGB(45, 49, 57));
        s_buttonBrush = CreateSolidBrush(RGB(50, 55, 64));
        s_rowBoundBrush = CreateSolidBrush(RGB(50, 100, 73));
        s_rowSelectedBrush = CreateSolidBrush(RGB(57, 62, 72));
        s_rowDefaultBrush = CreateSolidBrush(RGB(35, 38, 44));
    }
    FillRect(dc, &client, s_bgBrush);

    const ProductionStableId lockedMachine = ProductionGetActiveMachine();
    const bool hasLockedMachine = lockedMachine.mapId != 0 &&
                                  lockedMachine.uniqueId != 0;
    wchar_t titleText[192] = {};
    if (hasLockedMachine) {
        _snwprintf_s(
            titleText, sizeof(titleText) / sizeof(titleText[0]), _TRUNCATE,
            L"已锁定目标  地图 %llu ｜ #%06llX",
            static_cast<unsigned long long>(lockedMachine.mapId),
            static_cast<unsigned long long>(lockedMachine.uniqueId & 0xffffff));
    } else {
        wcscpy_s(titleText, L"生产设备自动化 v1.8 诊断");
    }
    RECT title = {ProductionScale(16), ProductionScale(12),
                  client.right - ProductionScale(64), ProductionScale(48)};
    ProductionDrawText(dc, g_productionPanelTitleFont,
                       hasLockedMachine ? RGB(102, 224, 151) :
                                          RGB(244, 246, 249),
                       titleText, title,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT closeButton = {client.right - ProductionScale(52),
                        ProductionScale(10),
                        client.right - ProductionScale(12),
                        ProductionScale(44)};
    FillRect(dc, &closeButton, s_closeBrush);
    ProductionDrawText(dc, g_productionPanelBodyFont, RGB(245, 245, 245),
                       L"X", closeButton,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT inputTab = {ProductionScale(14), ProductionScale(168),
                     ProductionScale(250), ProductionScale(204)};
    RECT outputTab = {ProductionScale(270), ProductionScale(168),
                      client.right - ProductionScale(14), ProductionScale(204)};
    FillRect(dc, &inputTab, g_productionPanelTab == 0 ? s_selectedBrush : s_normalBrush);
    FillRect(dc, &outputTab, g_productionPanelTab == 1 ? s_selectedBrush : s_normalBrush);
    ProductionDrawText(dc, g_productionPanelBodyFont, RGB(245, 245, 245),
                       L"输入箱", inputTab,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ProductionDrawText(dc, g_productionPanelBodyFont, RGB(245, 245, 245),
                       L"输出箱", outputTab,
                       DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    ProductionBinding binding = {};
    const bool hasBinding = ProductionGetActiveBinding(&binding);
    bool scanActive = false;
    size_t scanProcessed = 0;
    size_t scanTotal = 0;
    AcquireSRWLockShared(&g_productionPublishLock);
    scanActive = g_productionPublished.scanActive ||
        g_productionRegistryScanRequested.load(std::memory_order_acquire) ||
        g_productionScanInProgress.load(std::memory_order_acquire);
    scanProcessed = g_productionPublished.scanProcessed;
    scanTotal = g_productionPublished.scanTotal;
    ReleaseSRWLockShared(&g_productionPublishLock);
    // Status block first: the top of the left page shows the locked
    // device, its state, bound chests and the automation switch; the chest
    // selection and actions follow below.
    RECT statusBlock = {ProductionScale(14), ProductionScale(52),
                        client.right - ProductionScale(14), ProductionScale(164)};
    COLORREF statusColor = scanActive
        ? RGB(94, 158, 235)
        : (hasBinding ? ProductionStateAccent(binding.state, binding.enabled)
                      : RGB(231, 98, 98));
    // Cache 4 known status colors to avoid per-frame brush allocation
    static const COLORREF s_statusColors[4] = {
        RGB(94, 158, 235), RGB(231, 98, 98), RGB(80, 205, 118), RGB(244, 186, 84)
    };
    static HBRUSH s_statusBrushes[4] = {};
    if (!s_statusBrushes[0]) {
        for (int si = 0; si < 4; ++si)
            s_statusBrushes[si] = CreateSolidBrush(s_statusColors[si]);
    }
    int statusIdx = -1;
    for (int si = 0; si < 4; ++si) {
        if (statusColor == s_statusColors[si]) { statusIdx = si; break; }
    }
    if (statusIdx >= 0)
        FillRect(dc, &statusBlock, s_statusBrushes[statusIdx]);
    else {
        HBRUSH tmp = CreateSolidBrush(statusColor);
        FillRect(dc, &statusBlock, tmp);
        DeleteObject(tmp);
    }
    wchar_t statusLine1[256] = {};
    wchar_t statusLine2[256] = {};
    wchar_t statusLine3[256] = {};
    if (hasBinding) {
        _snwprintf_s(statusLine1,
                     sizeof(statusLine1) / sizeof(statusLine1[0]), _TRUNCATE,
                     L"状态:%ls ｜ 自动化:%ls",
                     ProductionStateNameZh(binding.state),
                     binding.enabled ? L"开" : L"关");
        _snwprintf_s(statusLine2,
                     sizeof(statusLine2) / sizeof(statusLine2[0]), _TRUNCATE,
                     L"输入箱:%ls（自动投料） ｜ 输出箱:%ls",
                     binding.hasInput ? L"已绑定" : L"未绑定",
                     binding.hasOutput ? L"已绑定" : L"未绑定");
        // Keep each format string paired with its exact variadic arguments.
        // The old conditional format always passed scanProcessed first; once
        // scanning finished, msvcrt interpreted that size_t (for example 976)
        // as the wchar_t* required by "原因:%ls".  The access violation then
        // escaped this Win32 callback as STATUS_FATAL_USER_CALLBACK_EXCEPTION.
        if (scanActive) {
            _snwprintf_s(
                statusLine3,
                sizeof(statusLine3) / sizeof(statusLine3[0]), _TRUNCATE,
                L"正在扫描附近箱子 %zu/%zu …", scanProcessed, scanTotal);
        } else {
            _snwprintf_s(
                statusLine3,
                sizeof(statusLine3) / sizeof(statusLine3[0]), _TRUNCATE,
                L"原因:%ls", ProductionReasonNameZh(binding.reason));
        }
    } else if (scanActive) {
        _snwprintf_s(statusLine1,
                     sizeof(statusLine1) / sizeof(statusLine1[0]), _TRUNCATE,
                     L"正在扫描附近箱子 %zu/%zu …", scanProcessed, scanTotal);
        wcscpy_s(statusLine2, L"请稍候");
        wcscpy_s(statusLine3, L"");
    } else {
        wcscpy_s(statusLine1, L"未解析到设备");
        wcscpy_s(statusLine2, L"面朝设备按 F3 / 十字键右锁定");
        wcscpy_s(statusLine3, L"输入侧搬运仍安全关闭（不写入物品）");
    }
    RECT statusLine1Rect = {ProductionScale(22), ProductionScale(58),
                            client.right - ProductionScale(20),
                            ProductionScale(92)};
    RECT statusLine2Rect = {ProductionScale(22), ProductionScale(94),
                            client.right - ProductionScale(20),
                            ProductionScale(126)};
    RECT statusLine3Rect = {ProductionScale(22), ProductionScale(128),
                            client.right - ProductionScale(20),
                            ProductionScale(158)};
    ProductionDrawText(dc, g_productionPanelBodyFont, RGB(255, 255, 255),
                       statusLine1, statusLine1Rect,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    ProductionDrawText(dc, g_productionPanelBodyFont, RGB(255, 255, 255),
                       statusLine2, statusLine2Rect,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    ProductionDrawText(dc, g_productionPanelSmallFont, RGB(255, 255, 255),
                       statusLine3, statusLine3Rect,
                       DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    AcquireSRWLockShared(&g_productionPublishLock);
    const size_t chestCount = g_productionPublished.chestCount;
    for (int row = 0; row < PRODUCTION_PANEL_ROWS; ++row) {
        const size_t index = g_productionPanelScroll + static_cast<size_t>(row);
        if (index >= chestCount) break;
        const ProductionObjectSnapshot& chest =
            g_productionPublished.chests[index];
        RECT rowRect = {ProductionScale(14), ProductionScale(208 + row * 32),
                        client.right - ProductionScale(14),
                        ProductionScale(238 + row * 32)};
        const bool bound = hasBinding &&
            ((g_productionPanelTab == 0 && binding.hasInput &&
              ProductionIdEqual(binding.input, chest.id)) ||
             (g_productionPanelTab == 1 && binding.hasOutput &&
              ProductionIdEqual(binding.output, chest.id)));
        FillRect(dc, &rowRect,
            bound ? s_rowBoundBrush :
            index == g_productionPanelSelection ? s_rowSelectedBrush :
                                                   s_rowDefaultBrush);
        wchar_t name[128] = {};
        wchar_t label[256] = {};
        ProductionUtf8ToWide(chest.name, name, 128);
        _snwprintf_s(label, 256, _TRUNCATE,
                     L"%ls ｜ 地图%llu ｜ #%06llX ｜ (%.0f, %.0f) ｜ 已解析%ls",
                     name, static_cast<unsigned long long>(chest.id.mapId),
                     static_cast<unsigned long long>(chest.id.uniqueId & 0xffffff),
                     chest.position[0], chest.position[1],
                     bound ? L"（已绑定）" : L"");
        ProductionDrawText(dc, g_productionPanelSmallFont,
                           RGB(235, 237, 240), label, rowRect,
                           DT_LEFT | DT_VCENTER | DT_SINGLELINE |
                               DT_END_ELLIPSIS);
    }
    ReleaseSRWLockShared(&g_productionPublishLock);

    RECT unbind = {ProductionScale(14), client.bottom - ProductionScale(72),
                   ProductionScale(184), client.bottom - ProductionScale(24)};
    RECT toggle = {ProductionScale(196), client.bottom - ProductionScale(72),
                   client.right - ProductionScale(14),
                   client.bottom - ProductionScale(24)};
    FillRect(dc, &unbind, s_buttonBrush);
    FillRect(dc, &toggle, s_buttonBrush);
    ProductionDrawText(dc, g_productionPanelBodyFont, RGB(245, 245, 245),
                       g_productionPanelTab == 0 ? L"解除输入箱绑定" :
                                                  L"解除输出箱绑定",
                       unbind, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ProductionDrawText(dc, g_productionPanelBodyFont, RGB(245, 245, 245),
                       hasBinding && binding.enabled ? L"暂停自动化" :
                                                       L"启用自动化",
                       toggle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    EndPaint(window, &paint);
}

static void ProductionRearmPanelInput() {
    g_productionKeyboardArmed = true;
    g_productionControllerArmed = true;
    g_productionPanelInputGeneration = 0;
    g_productionPanelPressedTarget = 0;
    g_productionPanelPressedRow = 0;
    g_productionPanelPressedGeneration = 0;
}

static void ProductionReleasePanelCapture() {
    if (g_productionPanelWindow &&
        GetCapture() == g_productionPanelWindow) ReleaseCapture();
    g_productionPanelPressedTarget = 0;
    g_productionPanelPressedRow = 0;
    g_productionPanelPressedGeneration = 0;
}

static bool ProductionPanelOwnerStillForeground() {
    HWND owner = g_productionPanelOwner;
    if (!owner || !IsWindow(owner) || !IsWindowVisible(owner) ||
        owner == g_productionPanelWindow) return false;
    DWORD ownerProcessId = 0;
    GetWindowThreadProcessId(owner, &ownerProcessId);
    const LONG_PTR extendedStyle = GetWindowLongPtrW(owner, GWL_EXSTYLE);
    if (ownerProcessId != GetCurrentProcessId() ||
        (extendedStyle & WS_EX_TOOLWINDOW) != 0 ||
        GetWindow(owner, GW_OWNER) != nullptr) return false;

    // The panel is a no-activate owned popup.  It must never move foreground
    // away from the game or try to steal it back after an external Alt-Tab.
    return GetForegroundWindow() == owner;
}

static constexpr u64 PRODUCTION_MODAL_CAPTURE_CURRENT_GENERATION = UINT64_MAX;

static bool ProductionHidePanelAndReleaseModal(
        const char* reason,
        u64 expectedGeneration = PRODUCTION_MODAL_CAPTURE_CURRENT_GENERATION,
        bool logLifecycle = true) {
    const u64 token = g_productionModalToken.load(std::memory_order_acquire);
    const u64 generation = ProductionModalGeneration(token);
    const u64 releaseGeneration =
        expectedGeneration == PRODUCTION_MODAL_CAPTURE_CURRENT_GENERATION
            ? generation : expectedGeneration;
    const bool wasBlocked =
        ProductionModalTokenState(token) != ProductionModalState::Hidden;
    const bool wasVisible = g_productionPanelWindow &&
        IsWindowVisible(g_productionPanelWindow);
    const bool released = ProductionReleaseModalInputBlock(releaseGeneration);
    if (released) {
        if (g_productionPanelWindow)
            ShowWindow(g_productionPanelWindow, SW_HIDE);
        ProductionReleasePanelCapture();
        g_productionPanelPublishedOwner.store(nullptr,
                                               std::memory_order_release);
        ProductionRearmPanelInput();
    }
    if (logLifecycle && (wasBlocked || wasVisible)) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=panel_lifecycle "
            "result=INCOMPLETE reason=%s panel_hidden=1 "
            "modal_generation=%llu modal_input_released=%d "
            "native_ui_calls=0\n",
            reason ? reason : "panel_hidden",
            static_cast<unsigned long long>(releaseGeneration),
            released ? 1 : 0);
    }
    return released;
}

static bool ProductionBeginPanelCloseDrain(
        u64 generation, unsigned closingMask, const char* reason,
        bool drainAllPhysical = false) {
    const u64 entered = g_productionMainCallbackEnteredSequence.load(
        std::memory_order_acquire);
    const u64 required = entered == UINT64_MAX ? UINT64_MAX : entered + 1;
    g_productionModalDrainRequiredCallback.store(
        required, std::memory_order_release);
    g_productionModalDrainCompletedGeneration.store(
        0, std::memory_order_release);
    g_productionModalDrainPhysicalGeneration.store(
        drainAllPhysical ? generation : 0, std::memory_order_release);
    if (!ProductionTransitionModalInputBlock(
            generation, ProductionModalState::Active,
            ProductionModalState::ClosingDrain, closingMask)) return false;
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=panel_lifecycle "
        "result=INCOMPLETE reason=%s modal_generation=%llu "
        "closing_mask=%u drain_all_physical=%d required_callback=%llu "
        "panel_hidden=0 "
        "modal_input_released=0 native_ui_calls=0\n",
        reason ? reason : "close_edge_drain",
        static_cast<unsigned long long>(generation), closingMask,
        drainAllPhysical ? 1 : 0,
        static_cast<unsigned long long>(required));
    return true;
}

static bool ProductionPanelActiveMachineValid() {
    if (!g_productionReady.load(std::memory_order_acquire) ||
        g_productionFaulted.load(std::memory_order_acquire)) return false;
    const ProductionStableId active = ProductionGetActiveMachine();
    if (!active.mapId || !active.uniqueId) return false;
    bool valid = false;
    AcquireSRWLockShared(&g_productionPublishLock);
    valid = g_productionPublished.activeMachinePresent &&
            ProductionIdEqual(g_productionPublished.activeMachine, active);
    ReleaseSRWLockShared(&g_productionPublishLock);
    return valid;
}

static bool ProductionSelectPublishedChest(size_t index) {
    const ProductionStableId active = ProductionGetActiveMachine();
    ProductionStableId id = {};
    char name[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    u64 generation = 0;
    bool scanActive = false;
    bool activeMatches = false;
    bool validChest = false;
    AcquireSRWLockShared(&g_productionPublishLock);
    generation = g_productionPublished.generation;
    scanActive = g_productionPublished.scanActive;
    activeMatches = g_productionPublished.activeMachinePresent &&
        ProductionIdEqual(g_productionPublished.activeMachine, active);
    if (index < g_productionPublished.chestCount) {
        const ProductionObjectSnapshot& candidate =
            g_productionPublished.chests[index];
        validChest = candidate.chest && candidate.named &&
                     candidate.inventoryValid;
        if (validChest) {
            id = candidate.id;
            strcpy_s(name, candidate.name);
        }
    }
    ReleaseSRWLockShared(&g_productionPublishLock);
    const u64 currentGeneration =
        g_productionSequence.load(std::memory_order_acquire);
    const char* rejectedReason = nullptr;
    if (!g_productionReady.load(std::memory_order_acquire) ||
        g_productionFaulted.load(std::memory_order_acquire)) {
        rejectedReason = "runtime_faulted";
    } else if (scanActive) {
        rejectedReason = "snapshot_scan_active";
    } else if (!generation || generation != currentGeneration ||
               !activeMatches) {
        rejectedReason = "snapshot_generation_stale";
    } else if (!validChest || !id.mapId || !id.uniqueId) {
        rejectedReason = "snapshot_chest_invalid";
    }
    if (rejectedReason) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=chest_select_guard "
            "result=FAIL reason=%s index=%zu generation=%llu "
            "current_generation=%llu scan_active=%d active_match=%d "
            "binding_changed=0 native_input_consumed=0\n",
            rejectedReason, index,
            static_cast<unsigned long long>(generation),
            static_cast<unsigned long long>(currentGeneration),
            scanActive ? 1 : 0, activeMatches ? 1 : 0);
        if (g_productionPanelWindow)
            InvalidateRect(g_productionPanelWindow, nullptr, FALSE);
        return false;
    }
    if (!ProductionMutateBinding(g_productionPanelTab, &id, name,
                                 generation)) return false;
    g_productionPanelSelection = index;
    return true;
}

enum ProductionPanelHitTarget : int {
    PRODUCTION_PANEL_HIT_NONE = 0,
    PRODUCTION_PANEL_HIT_CLOSE,
    PRODUCTION_PANEL_HIT_INPUT_TAB,
    PRODUCTION_PANEL_HIT_OUTPUT_TAB,
    PRODUCTION_PANEL_HIT_CHEST_ROW,
    PRODUCTION_PANEL_HIT_UNBIND,
    PRODUCTION_PANEL_HIT_TOGGLE,
};

static int ProductionPanelHitTest(int x, int y, size_t* chestIndex) {
    if (chestIndex) *chestIndex = 0;
    if (x >= g_productionPanelWidth - ProductionScale(52) &&
        x < g_productionPanelWidth - ProductionScale(12) &&
        y >= ProductionScale(10) && y < ProductionScale(44))
        return PRODUCTION_PANEL_HIT_CLOSE;
    if (y >= ProductionScale(168) && y < ProductionScale(204))
        return x < g_productionPanelWidth / 2
            ? PRODUCTION_PANEL_HIT_INPUT_TAB
            : PRODUCTION_PANEL_HIT_OUTPUT_TAB;
    if (y >= ProductionScale(208) &&
        y < ProductionScale(208 + PRODUCTION_PANEL_ROWS * 32)) {
        const size_t row = static_cast<size_t>(
            (y - ProductionScale(208)) / ProductionScale(32));
        if (chestIndex) *chestIndex = g_productionPanelScroll + row;
        return PRODUCTION_PANEL_HIT_CHEST_ROW;
    }
    if (y >= g_productionPanelHeight - ProductionScale(72) &&
        y < g_productionPanelHeight - ProductionScale(24))
        return x < ProductionScale(190)
            ? PRODUCTION_PANEL_HIT_UNBIND
            : PRODUCTION_PANEL_HIT_TOGGLE;
    return PRODUCTION_PANEL_HIT_NONE;
}

static LRESULT CALLBACK ProductionPanelWndProc(HWND window, UINT message,
                                               WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_MOUSEACTIVATE:
        // Keep the real game top-level active.  If another application already
        // took foreground but its deactivation message has not been pumped,
        // eat this edge instead of letting a stale topmost popup act on it.
        return ProductionForegroundIsPanelOwnerWindow()
            ? MA_NOACTIVATE : MA_NOACTIVATEANDEAT;
    case WM_ACTIVATEAPP:
        if (!wParam) {
            ProductionHidePanelAndReleaseModal("foreground_lost");
            ProductionMarkerClearAllowed();
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        ProductionPaintPanel(window);
        return 0;
    case WM_MOUSEWHEEL: {
        const u64 token = g_productionModalToken.load(
            std::memory_order_acquire);
        if (ProductionModalTokenState(token) != ProductionModalState::Active ||
            !IsWindowVisible(window) ||
            !ProductionForegroundIsPanelOwnerWindow() ||
            !ProductionPanelActiveMachineValid()) return 0;
        const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        AcquireSRWLockShared(&g_productionPublishLock);
        const size_t count = g_productionPublished.chestCount;
        ReleaseSRWLockShared(&g_productionPublishLock);
        if (delta < 0 && g_productionPanelScroll + PRODUCTION_PANEL_ROWS < count)
            ++g_productionPanelScroll;
        else if (delta > 0 && g_productionPanelScroll > 0)
            --g_productionPanelScroll;
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        const u64 token = g_productionModalToken.load(
            std::memory_order_acquire);
        if (ProductionModalTokenState(token) != ProductionModalState::Active ||
            !IsWindowVisible(window) ||
            !ProductionForegroundIsPanelOwnerWindow())
            return 0;
        const int x = static_cast<short>(LOWORD(lParam));
        const int y = static_cast<short>(HIWORD(lParam));
        size_t row = 0;
        const int target = ProductionPanelHitTest(x, y, &row);
        if (target != PRODUCTION_PANEL_HIT_CLOSE &&
            !ProductionPanelActiveMachineValid()) return 0;
        g_productionPanelPressedTarget = target;
        g_productionPanelPressedRow = row;
        g_productionPanelPressedGeneration =
            ProductionModalGeneration(token);
        SetCapture(window);
        if (GetCapture() != window) {
            g_productionPanelPressedTarget = PRODUCTION_PANEL_HIT_NONE;
            g_productionPanelPressedRow = 0;
            g_productionPanelPressedGeneration = 0;
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        const int pressedTarget = g_productionPanelPressedTarget;
        const size_t pressedRow = g_productionPanelPressedRow;
        const u64 pressedGeneration =
            g_productionPanelPressedGeneration;
        // Capture belongs to the whole modal generation, not one mouse edge.
        // Clear only the immutable press record here; releasing capture between
        // up and the worker's next pump would let a second edge reach the game.
        g_productionPanelPressedTarget = PRODUCTION_PANEL_HIT_NONE;
        g_productionPanelPressedRow = 0;
        g_productionPanelPressedGeneration = 0;
        const u64 token = g_productionModalToken.load(
            std::memory_order_acquire);
        if (ProductionModalTokenState(token) != ProductionModalState::Active ||
            ProductionModalGeneration(token) != pressedGeneration ||
            !pressedTarget || !IsWindowVisible(window) ||
            !ProductionForegroundIsPanelOwnerWindow()) return 0;
        const int x = static_cast<short>(LOWORD(lParam));
        const int y = static_cast<short>(HIWORD(lParam));
        size_t releasedRow = 0;
        const int releasedTarget = ProductionPanelHitTest(
            x, y, &releasedRow);
        if (releasedTarget != pressedTarget ||
            (pressedTarget == PRODUCTION_PANEL_HIT_CHEST_ROW &&
             releasedRow != pressedRow)) return 0;
        if (pressedTarget == PRODUCTION_PANEL_HIT_CLOSE) {
            ProductionBeginPanelCloseDrain(
                pressedGeneration, 0, "close_button_drain");
        } else if (!ProductionPanelActiveMachineValid()) {
            return 0;
        } else if (pressedTarget == PRODUCTION_PANEL_HIT_INPUT_TAB ||
                   pressedTarget == PRODUCTION_PANEL_HIT_OUTPUT_TAB) {
            g_productionPanelTab =
                pressedTarget == PRODUCTION_PANEL_HIT_INPUT_TAB ? 0 : 1;
            InvalidateRect(window, nullptr, FALSE);
        } else if (pressedTarget == PRODUCTION_PANEL_HIT_CHEST_ROW) {
            ProductionSelectPublishedChest(pressedRow);
        } else if (pressedTarget == PRODUCTION_PANEL_HIT_UNBIND) {
            ProductionMutateBinding(2 + g_productionPanelTab,
                                    nullptr, nullptr);
        } else if (pressedTarget == PRODUCTION_PANEL_HIT_TOGGLE) {
            ProductionMutateBinding(4, nullptr, nullptr);
        }
        return 0;
    }
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_MOUSEHWHEEL:
    case WM_MOUSEMOVE:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
    case WM_DEADCHAR:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
    case WM_UNICHAR:
    case WM_HOTKEY:
    case WM_INPUT_DEVICE_CHANGE:
    case WM_GESTURENOTIFY:
        return 0;
    case WM_APPCOMMAND:
        // TRUE marks the application command handled and prevents USER32 from
        // walking it up to the game owner window.
        return TRUE;
    case WM_INPUT:
        // RIM_INPUT requires DefWindowProc so USER32 can perform the raw-input
        // cleanup.  The message is still handled by this panel and is not
        // forwarded to the game window.
        return DefWindowProcW(window, message, wParam, lParam);
    case WM_GESTURE:
        // Let USER32 close the HGESTUREINFO handle.  Returning zero directly
        // here would leak it; this does not publish the gesture to the owner.
        return DefWindowProcW(window, message, wParam, lParam);
    case WM_CANCELMODE:
        ProductionHidePanelAndReleaseModal("panel_cancel_mode");
        ProductionMarkerClearAllowed();
        return 0;
    case WM_CAPTURECHANGED:
        if (reinterpret_cast<HWND>(lParam) != window) {
            g_productionPanelPressedTarget = PRODUCTION_PANEL_HIT_NONE;
            g_productionPanelPressedRow = 0;
            g_productionPanelPressedGeneration = 0;
            const u64 token = g_productionModalToken.load(
                std::memory_order_acquire);
            if (ProductionModalTokenState(token) !=
                    ProductionModalState::Hidden) {
                ProductionHidePanelAndReleaseModal(
                    "panel_capture_lost", ProductionModalGeneration(token));
                ProductionMarkerClearAllowed();
            }
        }
        return 0;
    case WM_CLOSE:
        {
            const u64 token = g_productionModalToken.load(
                std::memory_order_acquire);
            ProductionBeginPanelCloseDrain(
                ProductionModalGeneration(token), 0,
                "window_close_drain");
        }
        return 0;
    case WM_DESTROY:
        ProductionReleasePanelCapture();
        {
            const u64 token = g_productionModalToken.load(
                std::memory_order_acquire);
            ProductionReleaseModalInputBlock(
                ProductionModalGeneration(token));
        }
        ProductionRearmPanelInput();
        g_productionPanelWindow = nullptr;
        g_productionPanelPublishedWindow.store(
            nullptr, std::memory_order_release);
        g_productionPanelPublishedOwner.store(
            nullptr, std::memory_order_release);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

static bool ProductionInitPanel() {
    if (g_productionPanelWindow) return g_productionXInputGetState != nullptr;
    ProductionLoadConfig();
    g_productionPanelOwner = FindSettingsOwner();
    g_productionPanelWidth = ProductionScale(PRODUCTION_PANEL_BASE_WIDTH);
    g_productionPanelHeight = ProductionScale(PRODUCTION_PANEL_BASE_HEIGHT);
    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = ProductionPanelWndProc;
    cls.hInstance = g_module;
    cls.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    cls.lpszClassName = PRODUCTION_PANEL_CLASS;
    if (!RegisterClassExW(&cls)) {
        WNDCLASSEXW existing = {};
        existing.cbSize = sizeof(existing);
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS ||
            !GetClassInfoExW(g_module, PRODUCTION_PANEL_CLASS, &existing) ||
            existing.lpfnWndProc != ProductionPanelWndProc) return false;
    }
    g_productionPanelTitleFont = CreateFontW(
        -ProductionScale(20), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Microsoft YaHei UI");
    g_productionPanelBodyFont = CreateFontW(
        -ProductionScale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Microsoft YaHei UI");
    g_productionPanelSmallFont = CreateFontW(
        -ProductionScale(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Microsoft YaHei UI");
    if (!g_productionPanelTitleFont || !g_productionPanelBodyFont ||
        !g_productionPanelSmallFont) return false;
    g_productionPanelWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        PRODUCTION_PANEL_CLASS, L"生产设备自动化", WS_POPUP,
        0, 0, g_productionPanelWidth, g_productionPanelHeight,
        g_productionPanelOwner, nullptr, g_module, nullptr);
    if (!g_productionPanelWindow) return false;
    g_productionPanelPublishedWindow.store(
        g_productionPanelWindow, std::memory_order_release);
    SetLayeredWindowAttributes(g_productionPanelWindow, 0, 248, LWA_ALPHA);
    const wchar_t* xinputCandidates[] = {
        L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    char xinputGetStateName[15] = {};
    for (size_t index = 0; index < sizeof(xinputGetStateName); ++index) {
        xinputGetStateName[index] = static_cast<char>(
            g_productionXInputGetStateEncoded[index] ^ 0xa5u);
    }
    for (const wchar_t* candidate : xinputCandidates) {
        g_productionXInputModule = LoadLibraryW(candidate);
        if (!g_productionXInputModule) continue;
        g_productionXInputGetState =
            reinterpret_cast<ProductionXInputGetStateFunction>(GetProcAddress(
                g_productionXInputModule, xinputGetStateName));
        if (g_productionXInputGetState) break;
        FreeLibrary(g_productionXInputModule);
        g_productionXInputModule = nullptr;
    }
    SecureZeroMemory(xinputGetStateName, sizeof(xinputGetStateName));
    if (!g_productionXInputGetState) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=panel_init result=FAIL "
            "reason=xinput_unavailable native_input_code_hook=0\n");
        return false;
    }
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=panel_init result=INCOMPLETE "
        "reason=none no_activate=1 modal_foreground_window=0 "
        "mouse=1 mouse_capture=1 keyboard_close=f3 "
        "controller_close=dpad_right close_button=1 "
        "native_input_code_hook=0 callback_local_input_mask=1 "
        "game_iat_api_gate=1 directinput_factory_proxy_aw=1 "
        "directinput_target_interface=a directinput_device_proxy=1 "
        "dispatch_message_filter=1 keyboard_poll_filter=1 "
        "xinput_poll_filter=1 "
        "close_edge_drain=1 "
        "foreground_only=1 worker_native_calls=0 xinput=%d\n",
        g_productionXInputGetState ? 1 : 0);
    return true;
}

static bool ProductionShowPanel(u64 modalGeneration) {
    if (!ProductionModalApiIsolationIsReady() ||
        !ProductionInitPanel()) return false;
    const u64 token = g_productionModalToken.load(std::memory_order_acquire);
    if (!modalGeneration || ProductionModalGeneration(token) != modalGeneration ||
        (ProductionModalTokenState(token) != ProductionModalState::Opening &&
         ProductionModalTokenState(token) != ProductionModalState::Active))
        return false;
    const u64 openingOwnerGeneration =
        g_productionModalOpeningOwnerGeneration.load(
            std::memory_order_acquire);
    HWND openingOwner = g_productionModalOpeningOwner.load(
        std::memory_order_acquire);
    if (openingOwnerGeneration != modalGeneration || !openingOwner ||
        GetForegroundWindow() != openingOwner ||
        !ProductionForegroundIsGameWindow())
        return false;
    g_productionPanelOwner = openingOwner;
    if (g_productionPanelOwner) {
        SetLastError(ERROR_SUCCESS);
        const LONG_PTR previousOwner = SetWindowLongPtrW(
            g_productionPanelWindow, GWLP_HWNDPARENT,
            reinterpret_cast<LONG_PTR>(g_productionPanelOwner));
        if (previousOwner == 0 && GetLastError() != ERROR_SUCCESS) return false;
    }
    RECT anchor = {};
    if (!g_productionPanelOwner ||
        !GetWindowRect(g_productionPanelOwner, &anchor)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &anchor, 0);
    }
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    HMONITOR handle = MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(handle, &monitor)) monitor.rcWork = anchor;
    int x = anchor.left - g_productionPanelWidth - ProductionScale(8);
    if (x < monitor.rcWork.left) x = anchor.left + ProductionScale(8);
    int y = anchor.top + ProductionScale(16);
    if (y + g_productionPanelHeight > monitor.rcWork.bottom)
        y = monitor.rcWork.bottom - g_productionPanelHeight;
    if (y < monitor.rcWork.top) y = monitor.rcWork.top;
    if (g_productionModalOpeningOwnerGeneration.load(
            std::memory_order_acquire) != modalGeneration ||
        g_productionModalOpeningOwner.load(std::memory_order_acquire) !=
            openingOwner ||
        GetForegroundWindow() != openingOwner ||
        !ProductionForegroundIsGameWindow()) return false;
    g_productionPanelPublishedOwner.store(openingOwner,
                                          std::memory_order_release);
    if (!SetWindowPos(g_productionPanelWindow, HWND_TOPMOST, x, y,
                      g_productionPanelWidth, g_productionPanelHeight,
                      SWP_NOACTIVATE | SWP_SHOWWINDOW)) return false;
    SetCapture(g_productionPanelWindow);
    const u64 confirmedToken = g_productionModalToken.load(
        std::memory_order_acquire);
    const ProductionModalState confirmedState =
        ProductionModalTokenState(confirmedToken);
    if (ProductionModalGeneration(confirmedToken) != modalGeneration ||
        (confirmedState != ProductionModalState::Opening &&
         confirmedState != ProductionModalState::Active) ||
        GetForegroundWindow() != openingOwner ||
        !ProductionForegroundIsPanelOwnerWindow() ||
        GetCapture() != g_productionPanelWindow) {
        ShowWindow(g_productionPanelWindow, SW_HIDE);
        ProductionReleasePanelCapture();
        return false;
    }
    InvalidateRect(g_productionPanelWindow, nullptr, FALSE);
    return IsWindowVisible(g_productionPanelWindow) != FALSE;
}

static bool ProductionRawDpadRightDown() {
    if (!g_productionXInputGetState) return false;
    for (DWORD user = 0; user < XUSER_MAX_COUNT; ++user) {
        XINPUT_STATE state = {};
        if (g_productionXInputGetState(user, &state) == ERROR_SUCCESS &&
            (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0)
            return true;
    }
    return false;
}

static bool ProductionAllPhysicalPanelInputReleased() {
    for (int key = 1; key < 256; ++key) {
        if ((GetAsyncKeyState(key) & 0x8000) != 0) return false;
    }
    if (!g_productionXInputGetState) return false;
    for (DWORD user = 0; user < XUSER_MAX_COUNT; ++user) {
        XINPUT_STATE state = {};
        const DWORD result = g_productionXInputGetState(user, &state);
        if (result == ERROR_DEVICE_NOT_CONNECTED) continue;
        if (result != ERROR_SUCCESS) return false;
        const XINPUT_GAMEPAD& gamepad = state.Gamepad;
        if (gamepad.wButtons != 0 ||
            gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ||
            gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ||
            gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
            gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
            gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
            gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ||
            gamepad.sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
            gamepad.sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
            gamepad.sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE ||
            gamepad.sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
            return false;
    }
    return true;
}

static void ProductionPumpChordInput() {
    const u64 token = g_productionModalToken.load(std::memory_order_acquire);
    const u64 generation = ProductionModalGeneration(token);
    const ProductionModalState modalState = ProductionModalTokenState(token);
    const bool visible = g_productionPanelWindow &&
        IsWindowVisible(g_productionPanelWindow);
    if (modalState == ProductionModalState::Hidden) {
        if (visible) ShowWindow(g_productionPanelWindow, SW_HIDE);
        // Token teardown is authoritative even if the OS already hid/minimized
        // the window.  Never rely on that transition to release capture for us.
        ProductionReleasePanelCapture();
        ProductionRearmPanelInput();
        return;
    }
    const bool foregroundValid = visible
        ? ProductionForegroundIsPanelOwnerWindow()
        : ProductionForegroundSupportsProductionModal();
    if (!foregroundValid) {
        ProductionHidePanelAndReleaseModal(
            "foreground_poll_lost", generation);
        ProductionMarkerClearAllowed();
        return;
    }
    if (!visible &&
        (modalState != ProductionModalState::Opening ||
         (!g_productionPanelRequested.load(std::memory_order_acquire) &&
          !g_productionPanelOpenCommitInProgress.load(
              std::memory_order_acquire)))) {
        ProductionHidePanelAndReleaseModal(
            "visibility_lost", generation);
        ProductionMarkerClearAllowed();
        return;
    }
    if (!visible && modalState == ProductionModalState::Opening &&
        (g_productionPanelRequested.load(std::memory_order_acquire) ||
         g_productionPanelOpenCommitInProgress.load(
             std::memory_order_acquire))) {
        // The request is waiting for its opening callback to finish.  Do not
        // arm/close this generation before the panel is actually visible.
        return;
    }
    if (visible && GetCapture() != g_productionPanelWindow) {
        SetCapture(g_productionPanelWindow);
        if (GetCapture() != g_productionPanelWindow) {
            ProductionHidePanelAndReleaseModal(
                "mouse_capture_reacquire_failed", generation);
            ProductionMarkerClearAllowed();
            return;
        }
    }

    const bool keyboardDown =
        (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
    const bool controllerDown = ProductionRawDpadRightDown();
    if (g_productionPanelInputGeneration != generation) {
        g_productionPanelInputGeneration = generation;
        g_productionKeyboardArmed = false;
        g_productionControllerArmed = false;
        g_productionPanelPressedTarget = PRODUCTION_PANEL_HIT_NONE;
        g_productionPanelPressedGeneration = 0;
    }
    if (!keyboardDown) g_productionKeyboardArmed = true;
    if (!controllerDown) g_productionControllerArmed = true;

    if (modalState == ProductionModalState::Opening) {
        const unsigned openerMask = ProductionModalTokenMask(token);
        const bool openerReleased =
            ((openerMask & PRODUCTION_MODAL_KEYBOARD_F3) == 0 ||
             !keyboardDown) &&
            ((openerMask & PRODUCTION_MODAL_CONTROLLER_DPAD_RIGHT) == 0 ||
             !controllerDown);
        if (openerReleased)
            ProductionTransitionModalInputBlock(
                generation, ProductionModalState::Opening,
                ProductionModalState::Active, 0);
        return; // never interpret the opening release as a close edge
    }

    if (modalState == ProductionModalState::ClosingDrain) {
        const unsigned closingMask = ProductionModalTokenMask(token);
        const bool edgeReleased =
            ((closingMask & PRODUCTION_MODAL_KEYBOARD_F3) == 0 ||
             !keyboardDown) &&
            ((closingMask & PRODUCTION_MODAL_CONTROLLER_DPAD_RIGHT) == 0 ||
             !controllerDown);
        const bool callbackDrained =
            g_productionModalDrainCompletedGeneration.load(
                std::memory_order_acquire) == generation;
        const bool physicalReleased =
            g_productionModalDrainPhysicalGeneration.load(
                std::memory_order_acquire) != generation ||
            ProductionAllPhysicalPanelInputReleased();
        if (edgeReleased && callbackDrained && physicalReleased) {
            const bool panelHidden = ProductionHidePanelAndReleaseModal(
                "close_edge_drained", generation);
            const bool foregroundPreserved = panelHidden &&
                ProductionPanelOwnerStillForeground();
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=panel_lifecycle "
                "result=INCOMPLETE reason=close_foreground_preserved "
                "modal_generation=%llu panel_hidden=%d "
                "owner_foreground_preserved=%d native_ui_calls=0\n",
                static_cast<unsigned long long>(generation),
                panelHidden ? 1 : 0, foregroundPreserved ? 1 : 0);
            ProductionMarkerClearAllowed();
        }
        return;
    }

    unsigned closingMask = 0;
    if (g_productionKeyboardArmed && keyboardDown)
        closingMask |= PRODUCTION_MODAL_KEYBOARD_F3;
    if (g_productionControllerArmed && controllerDown)
        closingMask |= PRODUCTION_MODAL_CONTROLLER_DPAD_RIGHT;
    if (closingMask != 0) {
        if ((closingMask & PRODUCTION_MODAL_KEYBOARD_F3) != 0)
            g_productionKeyboardArmed = false;
        if ((closingMask & PRODUCTION_MODAL_CONTROLLER_DPAD_RIGHT) != 0)
            g_productionControllerArmed = false;
        ProductionBeginPanelCloseDrain(
            generation, closingMask, "close_shortcut_drain");
    }
}

static void ProductionPumpPanel() {
    // Bindings must exist even when the player never requests the mod panel.
    // This executes once on the mod worker thread and fails closed on damage.
    if (!g_productionConfigLoaded) ProductionLoadConfig();

    // No visual generation may exist before all exact game-IAT slots have
    // passed readback and the exported DirectInput factory proxy is live.  A
    // later isolation fault also tears down capture/token on this worker.
    if (!ProductionModalApiIsolationIsReady()) {
        g_productionPanelRequested.store(false, std::memory_order_release);
        g_productionPanelShowAfterCallbackSequence.store(
            0, std::memory_order_release);
        g_productionPanelHideRequested.store(false,
                                               std::memory_order_release);
        ProductionHidePanelAndReleaseModal(
            "input_api_isolation_fault",
            PRODUCTION_MODAL_CAPTURE_CURRENT_GENERATION, false);
        ProductionMarkerClearAllowed();
        return;
    }

    // The master F1 switch is authoritative even during the short interval
    // before the next game-main callback cancels any in-flight scan.  Hide and
    // disarm first, and do not dispatch window mutation messages while off.
    if (!ProductionFeatureIsEnabled()) {
        g_productionPanelRequested.store(false, std::memory_order_release);
        g_productionPanelShowAfterCallbackSequence.store(
            0, std::memory_order_release);
        g_productionPanelHideRequested.store(false, std::memory_order_release);
        ProductionHidePanelAndReleaseModal(
            "feature_disabled", PRODUCTION_MODAL_CAPTURE_CURRENT_GENERATION,
            false);
        ProductionMarkerClearAllowed();
        return;
    }

    // The same real game owner must retain foreground before and throughout
    // the no-activate panel lifetime.  This polling backstops WM_ACTIVATEAPP so
    // a stale request cannot reappear after Alt-Tab/focus loss.
    if (!ProductionForegroundSupportsProductionModal()) {
        g_productionPanelRequested.store(false, std::memory_order_release);
        g_productionPanelShowAfterCallbackSequence.store(
            0, std::memory_order_release);
        g_productionPanelHideRequested.store(false, std::memory_order_release);
        ProductionHidePanelAndReleaseModal(
            "foreground_poll_lost",
            PRODUCTION_MODAL_CAPTURE_CURRENT_GENERATION, false);
        ProductionMarkerClearAllowed();
        return;
    }

    const ULONGLONG now = GetTickCount64();
    const bool callbackInFlight =
        g_productionMainCallbackInFlight.load(std::memory_order_acquire);
    const ULONGLONG callbackEntered =
        g_productionMainCallbackEnteredTick.load(std::memory_order_acquire);
    const ULONGLONG callbackCompleted =
        g_productionMainCallbackCompletedTick.load(std::memory_order_acquire);
    const ULONGLONG lastWorldCallback = callbackInFlight
        ? callbackEntered : callbackCompleted;
    const ULONGLONG callbackAge =
        lastWorldCallback && now >= lastWorldCallback
            ? now - lastWorldCallback
            : 0;
    const bool worldRuntimeReady =
        g_productionWorldContextActive.load(std::memory_order_acquire) &&
        g_productionReady.load(std::memory_order_acquire) &&
        !g_productionFaulted.load(std::memory_order_acquire);
    // An entered callback is live no matter how long its scan/native work
    // takes.  The worker may report it, but must never hide the panel or
    // release modal input on an in-flight timestamp.
    const bool worldCallbackFresh = lastWorldCallback != 0 &&
        worldRuntimeReady &&
        (callbackInFlight ||
         callbackAge <= PRODUCTION_PANEL_WORLD_CALLBACK_STALE_MS);
    const ProductionStableId activeBeforeTimeout = ProductionGetActiveMachine();
    const bool activePresent = activeBeforeTimeout.mapId != 0 &&
                               activeBeforeTimeout.uniqueId != 0;
    const bool panelVisible = g_productionPanelWindow &&
                              IsWindowVisible(g_productionPanelWindow);
    const bool requestPending =
        g_productionPanelRequested.load(std::memory_order_acquire);
    const u64 showAfterCallbackSequence =
        g_productionPanelShowAfterCallbackSequence.load(
            std::memory_order_acquire);
    const u64 completedCallbackSequence =
        g_productionMainCallbackCompletedSequence.load(
            std::memory_order_acquire);
    const bool requestShowBarrierSatisfied = requestPending &&
        showAfterCallbackSequence != 0 && !callbackInFlight &&
        completedCallbackSequence >= showAfterCallbackSequence;
    const bool modalBlocked = ProductionModalInputIsBlocked();
    const bool lifecycleActive =
        activePresent || panelVisible || requestPending || modalBlocked;
    if (callbackInFlight &&
        callbackAge > PRODUCTION_PANEL_WORLD_CALLBACK_STALE_MS &&
        lifecycleActive) {
        // Slow in-flight callbacks are diagnostic only.  In particular, the
        // normal full scan can legitimately exceed the old 500 ms threshold.
        // No panel/capture/modal lifecycle operation is legal in this branch.
        g_productionWorkerTimeoutObserved.store(true,
                                                 std::memory_order_release);
        if (!g_productionPanelInFlightSlowLogged) {
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=panel_lifecycle "
                "result=INCOMPLETE reason=world_callback_in_flight_slow "
                "callback_present=1 callback_age_ms=%llu stale_ms=%llu "
                "callback_in_flight=1 callback_phase=%u "
                "pre_update_stage=%u panel_hidden=0 drain_started=0 "
                "modal_input_released=0 active_cleared=0 "
                "request_cleared=0 native_input_consumed=0\n",
                static_cast<unsigned long long>(callbackAge),
                static_cast<unsigned long long>(
                    PRODUCTION_PANEL_WORLD_CALLBACK_STALE_MS),
                g_productionMainCallbackPhase.load(
                    std::memory_order_acquire),
                g_productionPreUpdateStage.load(
                    std::memory_order_acquire));
        }
        g_productionPanelInFlightSlowLogged = true;
    } else if (!callbackInFlight && !worldCallbackFresh && lifecycleActive) {
        // Grade one closes through ClosingDrain so a resumed game callback
        // consumes the last blocked edge.  Only the much later hard threshold
        // performs physical fail-safe teardown when no callback ever resumes.
        // Neither grade mutates the main-thread request/scan/active lifecycle.
        g_productionWorkerTimeoutObserved.store(true,
                                                 std::memory_order_release);
        const u64 timeoutToken = g_productionModalToken.load(
            std::memory_order_acquire);
        const u64 timeoutGeneration =
            ProductionModalGeneration(timeoutToken);
        const bool hardRelease = lastWorldCallback != 0 &&
            callbackAge >=
                PRODUCTION_PANEL_WORLD_CALLBACK_HARD_RELEASE_MS;
        bool drainStarted = false;
        bool modalReleased = false;
        bool performedHardRelease = false;
        if (hardRelease) {
            modalReleased = ProductionHidePanelAndReleaseModal(
                "world_callback_hard_timeout", timeoutGeneration, false);
            performedHardRelease = !g_productionPanelHardReleaseLogged;
            g_productionPanelHardReleaseLogged = true;
        } else if (!hardRelease &&
                   ProductionModalTokenState(timeoutToken) ==
                       ProductionModalState::Active) {
            drainStarted = ProductionBeginPanelCloseDrain(
                timeoutGeneration, 0, "world_callback_stale_drain", true);
        }
        ProductionMarkerClearAllowed();
        if (!g_productionPanelWorldTimedOut) {
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=panel_lifecycle "
                "result=INCOMPLETE reason=world_callback_stale "
                "callback_present=%d callback_age_ms=%llu stale_ms=%llu "
                "hard_release_ms=%llu callback_in_flight=0 "
                "callback_phase=%u pre_update_stage=%u "
                "panel_hidden=%d drain_started=%d hard_release=%d "
                "active_cleared=0 request_cleared=0 "
                "modal_generation=%llu modal_input_released=%d "
                "native_input_consumed=0\n",
                lastWorldCallback ? 1 : 0,
                static_cast<unsigned long long>(callbackAge),
                static_cast<unsigned long long>(
                    PRODUCTION_PANEL_WORLD_CALLBACK_STALE_MS),
                static_cast<unsigned long long>(
                    PRODUCTION_PANEL_WORLD_CALLBACK_HARD_RELEASE_MS),
                g_productionMainCallbackPhase.load(
                    std::memory_order_acquire),
                g_productionPreUpdateStage.load(
                    std::memory_order_acquire),
                modalReleased ? 1 : 0, drainStarted ? 1 : 0,
                hardRelease ? 1 : 0,
                static_cast<unsigned long long>(timeoutGeneration),
                modalReleased ? 1 : 0);
        }
        if (performedHardRelease && g_productionPanelWorldTimedOut) {
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=panel_lifecycle "
                "result=INCOMPLETE reason=world_callback_hard_timeout "
                "callback_age_ms=%llu hard_release_ms=%llu "
                "callback_in_flight=0 panel_hidden=1 "
                "active_cleared=0 request_cleared=0 "
                "modal_generation=%llu modal_input_released=%d "
                "native_input_consumed=0\n",
                static_cast<unsigned long long>(callbackAge),
                static_cast<unsigned long long>(
                    PRODUCTION_PANEL_WORLD_CALLBACK_HARD_RELEASE_MS),
                static_cast<unsigned long long>(timeoutGeneration),
                modalReleased ? 1 : 0);
        }
        g_productionPanelWorldTimedOut = true;
    } else if (worldCallbackFresh) {
        g_productionPanelWorldTimedOut = false;
        g_productionPanelInFlightSlowLogged = false;
        g_productionPanelHardReleaseLogged = false;
        if (g_productionPanelHideRequested.exchange(
                false, std::memory_order_acq_rel)) {
            ProductionHidePanelAndReleaseModal(
                "main_lifecycle_hide",
                PRODUCTION_MODAL_CAPTURE_CURRENT_GENERATION, false);
        }
        if (requestShowBarrierSatisfied &&
            g_productionPanelRequested.exchange(
                false, std::memory_order_acq_rel)) {
            g_productionPanelShowAfterCallbackSequence.store(
                0, std::memory_order_release);
            u64 requestGeneration =
                g_productionPanelRequestGeneration.load(
                    std::memory_order_acquire);
            u64 requestToken = g_productionModalToken.load(
                std::memory_order_acquire);
            // A timeout/focus/lifecycle release invalidates this exact visual
            // request.  Never let the worker manufacture a new modal token:
            // only a fresh, main-thread-validated input edge may acquire one.
            // Active binding state remains value-only and can be reopened by
            // the player without resurrecting a stale owner/generation.
            const ProductionStableId locked = ProductionGetActiveMachine();
            wchar_t confirmation[160] = {};
            _snwprintf_s(
                confirmation,
                sizeof(confirmation) / sizeof(confirmation[0]), _TRUNCATE,
                L"已锁定面前生产设备  地图 %llu ｜ #%06llX",
                static_cast<unsigned long long>(locked.mapId),
                static_cast<unsigned long long>(locked.uniqueId & 0xffffff));
            if (!locked.mapId || !locked.uniqueId) {
                ProductionHidePanelAndReleaseModal(
                    "active_identity_missing", requestGeneration, false);
                ProductionLog(
                    "[PRODAUTO] seq=%llu txn=0 event=target_visual "
                    "result=FAIL reason=active_identity_missing "
                    "device_map_id=0 device_id=0 panel_visible=0 "
                    "marker_kind=mod_owned_floating_chip marker_visible=0 "
                    "native_object_glow=0 native_ui_calls=0\n");
            } else if (requestGeneration != 0 &&
                       ProductionModalGeneration(requestToken) ==
                           requestGeneration &&
                       ProductionShowPanel(requestGeneration)) {
                g_productionMarkerAllowed = true;
                ProductionRefreshMarker();
                const bool markerVisible = g_productionMarkerAllowed &&
                    g_productionMarkerWindow &&
                    IsWindowVisible(g_productionMarkerWindow);
                ShowToast(confirmation, RGB(80, 205, 118));
                ProductionLog(
                    "[PRODAUTO] seq=%llu txn=0 event=target_visual "
                    "result=INCOMPLETE reason=mod_owned_visual_confirmation "
                    "device_map_id=%llu device_id=%llu "
                    "visual=status_chip_toast_and_panel_title panel_visible=1 "
                    "marker_kind=mod_owned_floating_chip marker_visible=%d "
                    "native_object_glow=0 native_ui_calls=0\n",
                    static_cast<unsigned long long>(locked.mapId),
                    static_cast<unsigned long long>(locked.uniqueId),
                    markerVisible ? 1 : 0);
            } else {
                ProductionHidePanelAndReleaseModal(
                    "panel_show_failed", requestGeneration, false);
                ProductionLog(
                    "[PRODAUTO] seq=%llu txn=0 event=target_visual "
                    "result=FAIL reason=panel_show_failed "
                    "device_map_id=%llu device_id=%llu panel_visible=0 "
                    "marker_kind=mod_owned_floating_chip marker_visible=0 "
                    "native_object_glow=0 native_ui_calls=0\n",
                    static_cast<unsigned long long>(locked.mapId),
                    static_cast<unsigned long long>(locked.uniqueId));
            }
        }
    }
    // The marker chip is refreshed on every worker iteration while the
    // world context is fresh: binding/scan changes must be visible without
    // another lock.  It hides itself when the lock is gone or the context
    // timed out.
    ProductionRefreshMarker();
    if (g_productionPanelWindow) {
        // Reconcile a main-thread force-release before dispatching any queued
        // window input.  WndProc is value-only and fail-closed too, but this
        // removes the transient visible/captured-with-Hidden-token window.
        ProductionPumpChordInput();
        MSG message = {};
        while (PeekMessageW(&message, g_productionPanelWindow, 0, 0,
                            PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        ProductionPumpChordInput();
    }
    if (g_productionMarkerWindow) {
        MSG message = {};
        while (PeekMessageW(&message, g_productionMarkerWindow, 0, 0,
                            PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}
