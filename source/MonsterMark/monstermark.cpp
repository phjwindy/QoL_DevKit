// monstermark.cpp —— MonsterMark: Ghost Rat / Treasure Box 地图标记 (v1.0.26)
//
// v1.0.26: 生产路径 6 处原生函数调用补 SEH 保护（NightCollectTargets 等）。
//
// 从 dinput8.cpp 原始 .inl 提取的独立 DLL 插件。
// 实现逻辑见 night_map_markers.inl（已更新至 build 25094764 v1.09 RVA + byte array）。
//
// 本文件提供 .inl 所需的全部外部依赖桩函数。
// .inl 在文件末尾通过 #include 引入。

#define MONSTERMARK_VERSION L"v1.0.26"

#include <windows.h>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <cmath>
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
#include "selfverify.h"

// MonsterMark 日志开关：发布版禁用日志
// 诊断版（F7 callback 暴力扫描）必须开启
// #define MONSTERMARK_LOGGING
#ifdef MONSTERMARK_LOGGING
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
// RVA_GAME_ROOT — .inl 通过 RVA_GAME_ROOT 引用（与 ChestSort 一致）
// ============================================================
static constexpr uintptr_t RVA_GAME_ROOT = 0x10D4950;  // build 25094764

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
// WriteCodePatchChecked — 安全代码补丁（与 code_patch_safety.inl 一致）
// ============================================================
static bool WriteCodePatchChecked(void* address, const void* bytes,
                                  size_t size) {
    if (!PackageWritesAuthorized() ||
        !address || !bytes || size == 0) return false;
    DWORD previousProtection = 0;
    if (!VirtualProtect(address, size, PAGE_EXECUTE_READWRITE,
                        &previousProtection)) return false;
    memcpy(address, bytes, size);
    const bool flushed = FlushInstructionCache(
        GetCurrentProcess(), address, size) != 0;
    DWORD temporaryProtection = 0;
    const bool restored = VirtualProtect(
        address, size, PAGE_EXECUTE_READ, &temporaryProtection) != 0;
    return flushed && restored;
}

// ============================================================
// SealExecutableMemory — 将内存标记为 RX
// ============================================================
static bool SealExecutableMemory(void* address, size_t size) {
    DWORD previousProtection = 0;
    if (!PackageWritesAuthorized() ||
        !address || size == 0 ||
        !VirtualProtect(address, size, PAGE_EXECUTE_READ,
                        &previousProtection)) return false;
    return FlushInstructionCache(GetCurrentProcess(), address, size) != 0;
}

// ============================================================
// CodeDirectCallTargets — 验证 E8 call 目标
// ============================================================
static bool CodeDirectCallTargets(uintptr_t callSite,
                                  uintptr_t expectedTarget) {
    if (!IsReadable(reinterpret_cast<void*>(callSite), 5) ||
        *reinterpret_cast<const unsigned char*>(callSite) != 0xe8) return false;
    std::int32_t displacement = 0;
    memcpy(&displacement, reinterpret_cast<void*>(callSite + 1),
           sizeof(displacement));
    return callSite + 5 + static_cast<intptr_t>(displacement) == expectedTarget;
}

// ============================================================
// CodeRangeHasExpectedProtection — 验证地址范围页保护
// ============================================================
static bool CodeRangeHasExpectedProtection(void* address, size_t size) {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi) ||
        mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD) != 0 ||
        (mbi.Protect & 0xff) != PAGE_EXECUTE_READ) return false;
    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t regionStart =
        reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    if (mbi.RegionSize > UINTPTR_MAX - regionStart ||
        size > UINTPTR_MAX - start) return false;
    return start >= regionStart && start + size <= regionStart + mbi.RegionSize;
}

// ============================================================
// AllocateExecutableNear — 在 target ±2GB 范围分配可执行内存
// ============================================================
static void* AllocateExecutableNear(uintptr_t target, size_t size) {
    SYSTEM_INFO info = {};
    GetSystemInfo(&info);
    const uintptr_t granularity = info.dwAllocationGranularity;
    const uintptr_t range = 0x7fff0000ull;
    const uintptr_t minimum = target > range ? target - range :
        reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
    const uintptr_t maximumByRange = target + range;
    const uintptr_t maximumSystem =
        reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress);
    const uintptr_t maximum = maximumByRange < maximumSystem ?
        maximumByRange : maximumSystem;

    uintptr_t address = minimum & ~(granularity - 1);
    while (address < maximum) {
        MEMORY_BASIC_INFORMATION region = {};
        if (VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) == 0) {
            break;
        }
        const uintptr_t regionBase = reinterpret_cast<uintptr_t>(region.BaseAddress);
        const uintptr_t regionEnd = regionBase + region.RegionSize;
        if (region.State == MEM_FREE) {
            const uintptr_t candidate =
                (regionBase + granularity - 1) & ~(granularity - 1);
            if (candidate >= minimum && candidate + size <= regionEnd &&
                candidate + size <= maximum) {
                void* allocated = VirtualAlloc(reinterpret_cast<void*>(candidate), size,
                                               MEM_COMMIT | MEM_RESERVE,
                                               PAGE_READWRITE);
                if (allocated) {
                    const std::int64_t displacement =
                        static_cast<std::int64_t>(reinterpret_cast<uintptr_t>(allocated)) -
                        static_cast<std::int64_t>(target + 5);
                    if (displacement >= INT32_MIN && displacement <= INT32_MAX) {
                        return allocated;
                    }
                    VirtualFree(allocated, 0, MEM_RELEASE);
                }
            }
        }
        if (regionEnd <= address) break;
        address = regionEnd;
    }
    return nullptr;
}

// ============================================================
// SpatialTestFunction — .inl 通过 g_originalSpatialTest 引用
// ============================================================
using SpatialTestFunction = bool (__fastcall *)(void*, void*, void*, int, int);
static SpatialTestFunction g_originalSpatialTest = nullptr;

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
        Log("[MonsterMark] VerifyExeBuild: CreateFileW failed (err=%lu path=%ls)\n",
            GetLastError(), exePath);
        return false;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart != 18134536) {
        CloseHandle(hFile);
        return false;
    }

    // SHA-256 via BCrypt
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

    // build 25094764 (v1.09) SHA-256 — exe 二进制与 24969282 相同，仅 build 号变化
    static const unsigned char expected[32] = {
        0x7D, 0xC5, 0xAD, 0x61, 0x45, 0x41, 0xFB, 0x77,
        0x08, 0xE4, 0x20, 0x36, 0xDF, 0x7B, 0x7D, 0xA8,
        0x50, 0x53, 0xAB, 0x49, 0x59, 0xF6, 0xED, 0x2E,
        0x9D, 0xFE, 0x13, 0x30, 0xAE, 0xDB, 0xD3, 0x81
    };

    bool match = memcmp(hash, expected, sizeof(hash)) == 0;
    if (match) {
        Log("[MonsterMark] village.exe verified: build 25094764 (1.09)\n");
    } else {
        Log("[MonsterMark] village.exe SHA-256 mismatch; feature disabled\n");
    }
    return match;
}

// ============================================================
// 前向声明（实现在 night_map_markers.inl 中）
// ============================================================
static bool InstallNightMapMarkers();
static bool InstallNightAnchorPosHook();
static void NightDiagScanCallbacks();
static void NightDiagEnumerateHashTable();

// ============================================================
// 插件入口
// ============================================================

extern "C" __declspec(dllexport) void mod_init(void) {
    LogOpen("monstermark");
    if (!SelfVerifyInit("monstermark")) return;
    Log("[MonsterMark] mod_init — build 25094764 v1.09 %S\n", MONSTERMARK_VERSION);
    QolRegisterHotKey("monstermark", "none");  // 无热键（自动生效）

    g_supportedExe = VerifyExeBuild();
    if (!g_supportedExe) {
        Log("[MonsterMark] unsupported exe; feature disabled safely\n");
        return;
    }

    PackageOpenWriteGate();

    bool ready = InstallNightMapMarkers();
    Log("[MonsterMark] InstallNightMapMarkers = %s\n", ready ? "OK" : "FAILED");
    if (ready) {
        bool anchorReady = InstallNightAnchorPosHook();
        Log("[MonsterMark] InstallNightAnchorPosHook = %s\n",
            anchorReady ? "OK" : "FAILED");
    }
}

extern "C" __declspec(dllexport) void mod_tick(void) {
    // NightMapMarkers 是 hook-based：安装后由游戏原生 redraw 驱动
    // 诊断版：F7 边沿触发 callback 暴力扫描（仅诊断版使用）
    static bool s_f7NeedsRelease = true;
    bool f7Down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    bool f7Pressed = false;
    if (s_f7NeedsRelease) {
        if (!f7Down) s_f7NeedsRelease = false;
    } else if (f7Down) {
        f7Pressed = true;
        s_f7NeedsRelease = true;
    }
    // F7: enumerate g_gimmickMgr hash table to find all live gimmicks
    // (0x250690 direct call rejected — all SEH due to missing TLS context)
    if (f7Pressed) {
        Log("[MonsterMark] F7 pressed - enumerating gimmickMgr hash table\n");
        NightDiagEnumerateHashTable();
        Log("[MonsterMark] F7 hash table enumeration complete\n");
        Log("[MonsterMark] F7 starting callback scan\n");
        NightDiagScanCallbacks();
        Log("[MonsterMark] F7 callback scan finished\n");
    }
}

BOOL APIENTRY DllMain(HMODULE /*h*/, DWORD reason, LPVOID /*lpReserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        // 初始化推迟到 mod_init
    }
    return TRUE;
}

// ============================================================
// 引入 .inl 实现（所有外部依赖已在上方提供）
// ============================================================
#include "night_map_markers.inl"
