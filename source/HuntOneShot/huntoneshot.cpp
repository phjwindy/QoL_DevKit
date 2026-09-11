// huntoneshot.cpp —— 狩猎一击必杀 (HuntOneShot v1.0.0 for v1.09)
//
// 功能（移植自 HuntOneShot v0.2.0 by gloaming，1.08.1 → 1.09 重新适配）：
//   - 可狩猎动物（松鼠/狸猫/鼬鼠/兔子/狐狸/绿雉/野猪/鹿/熊）一枪狩猎成功
//   - 不影响掉落物与狩猎结算，动物仍正常逃跑/消失
//
// 机制（静态分析 huntoneshot.dll + 1.09 exe 逆向）：
//   - 锚点：
//       vt_hunt : 可狩猎动物 vtable（1.09 RVA 0xE055C8，lea 目标双重验证）
//       clock   : 全局对象读链（1.09 RVA 0x10D4950，mov rdx,[rip+disp] 双重验证）
//       vt_sub  : 第二 vtable（1.08.1 0xE47F80），1.09 签名失效 -> 运行时动态发现
//   - 扫描：遍历动物实例列表 -> 读 [inst+0x10] 高 32 位（初始命中）
//     白名单 {30,40,50,150,200,500}（松鼠30/狸猫·鼬鼠·兔子40/狐狸·绿雉50/野猪150/鹿200/熊500）
//     -> 命中则低 32 位 clamp 到 10
//   - 跨日重扫：天数变化时重新枚举
//
// 待实测项（v1.0.0）：
//   - 动物实例列表的枚举来源（链表/数组），先用候选偏移探测，日志输出实测确认后回填

#include <windows.h>
#include <cstdint>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "logging.h"
#include "selfverify.h"

// 日志开关：诊断版开启
#define HUNTONESHOT_LOGGING
#ifdef HUNTONESHOT_LOGGING
#else
  #define LogOpen(x)  ((void)0)
  #define Log(...)    ((void)0)
  #define LogV(...)   ((void)0)
  #define LogClose()  ((void)0)
#endif

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u8  = std::uint8_t;

// ============================================================
// 常量（v1.09）
// ============================================================

// ---- 锚点 RVA ----
static constexpr uintptr_t RVA_VT_HUNT_LEA = 0xDDD95;   // lea rax,[rip+..]（引用动物 vtable）
static constexpr uintptr_t RVA_CLOCK_LEA    = 0xD5D6A;   // mov rdx,[rip+..]（全局时钟指针）
static constexpr uintptr_t RVA_VT_HUNT      = 0xE055C8;  // 1.09 可狩猎动物 vtable（lea 目标验证）

// ---- clock 读链（与 MineHelper/AutoPet 一致）----
static constexpr uintptr_t CLOCK_SAVE_0x208 = 0x208;      // save = base + 0x208
static constexpr uintptr_t SAVE_RAW_SECOND  = 0x3270;     // save + 0x3270 = raw_second (i64)

// ---- 动物实例偏移 ----
static constexpr uintptr_t INST_VTABLE      = 0x00;       // inst + 0x0 = vtable 指针
static constexpr uintptr_t INST_HIT_PAIR    = 0x10;       // inst + 0x10 = 高32位初始命中 | 低32位当前

// ---- 白名单（初始命中次数，对应物种） ----
static const u32 kHitWhitelist[6] = {30, 40, 50, 150, 200, 500};
static constexpr u32 kClampValue  = 10;                    // clamp 目标值

// ---- 动物链表候选偏移（需实测回填） ----
// v1.0.0 先探测多个候选（与家畜同表或邻近表），日志确认后保留正确偏移
static const uintptr_t kListCandidates[] = {
    0x3400, 0x3410, 0x3420, 0x3430, 0x3440, 0x3450, 0x3460,
    0x3500, 0x3510, 0x3520, 0x3530, 0x3540,
};

// ============================================================
// 运行时状态
// ============================================================
static uintptr_t g_exeBase = 0;
static volatile LONG g_running = 0;
static u64 g_lastDay = 0;
static u64 g_variantHunt = RVA_VT_HUNT;   // 主猎物 vtable
static u64 g_vtDiscovered[8] = {0};       // 动态发现的其他猎物 vtable
static int  g_vtCount = 0;
static int g_totalClamped = 0;
static int g_processedCount = 0;
static bool g_enumerated = false;

// ============================================================
// 基础工具
// ============================================================
static bool ReadPtr(void* p, void** out) {
    if (!p) return false;
    uintptr_t v = 0;
    __try {
        v = *reinterpret_cast<uintptr_t*>(p);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    if (v < 0x10000) return false;
    *out = reinterpret_cast<void*>(v);
    return true;
}

static bool IsWhitelisted(u32 hit) {
    for (int i = 0; i < 6; ++i)
        if (kHitWhitelist[i] == hit) return true;
    return false;
}

static bool IsKnownVt(u64 vt) {
    if (vt == g_variantHunt) return true;
    for (int i = 0; i < g_vtCount; ++i)
        if (g_vtDiscovered[i] == vt) return true;
    return false;
}

// ============================================================
// 实例处理：白名单 -> clamp 低 32 位
// ============================================================
static void ProcessInstance(void* inst) {
    if (!inst) return;

    u64 vt = 0;
    __try {
        vt = *reinterpret_cast<u64*>(inst);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }

    // vtable 过滤：主 vtable 或已发现 sub
    if (!IsKnownVt(vt)) return;

    // 读 [inst+0x10] 高低 32 位
    u64 pair = 0;
    __try {
        pair = *reinterpret_cast<u64*>(
            reinterpret_cast<unsigned char*>(inst) + INST_HIT_PAIR);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }

    u32 init = static_cast<u32>(pair >> 32);   // 高 32 位 = 初始命中
    if (!IsWhitelisted(init)) return;

    u32 cur = static_cast<u32>(pair & 0xFFFFFFFF);  // 低 32 位 = 当前命中
    if (cur != kClampValue) {
        u64 newPair = (static_cast<u64>(init) << 32) | kClampValue;
        __try {
            *reinterpret_cast<u64*>(
                reinterpret_cast<unsigned char*>(inst) + INST_HIT_PAIR) = newPair;
            ++g_totalClamped;
            Log("[ho] clamp %p: %u -> %u (init=%u)\n", inst, cur, kClampValue, init);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[ho] clamp FAIL %p\n", inst);
        }
    }

    // 动态发现 sub vtable（1.09 无静态锚点，运行中记录非主 vtable 的猎物）
    if (vt != g_variantHunt) {
        bool known = false;
        for (int i = 0; i < g_vtCount; ++i)
            if (g_vtDiscovered[i] == vt) { known = true; break; }
        if (!known && g_vtCount < 8) {
            g_vtDiscovered[g_vtCount++] = vt;
            Log("[ho] discovered sub vtable=%08X\n", (unsigned)(vt & 0xFFFFFFFF));
        }
    }
}

// ============================================================
// 枚举：从 save 扫描候选列表
// ============================================================
static void ScanAnimals(void* save) {
    if (!save) return;
    int total = 0;

    // 候选偏移探测（v1.0.0 诊断）：找含 vtable 的链表节点
    for (size_t ci = 0; ci < sizeof(kListCandidates) / sizeof(kListCandidates[0]); ++ci) {
        unsigned char* listAddr = reinterpret_cast<unsigned char*>(save) + kListCandidates[ci];
        void* sentinel = nullptr;
        if (!ReadPtr(listAddr, &sentinel)) continue;
        if (!sentinel || reinterpret_cast<uintptr_t>(sentinel) < 0x10000) continue;

        u64 declared = 0;
        __try {
            declared = *reinterpret_cast<u64*>(listAddr + 8);
        } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        if (declared == 0 || declared > 4096) continue;

        // 校验 sentinel 双向链
        void* first = nullptr;
        if (!ReadPtr(sentinel, &first)) continue;
        void* back = nullptr;
        __try {
            back = *reinterpret_cast<void**>(
                reinterpret_cast<unsigned char*>(first) + 8);
        } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        if (back != sentinel) continue;  // 非标准双向链

        // 遍历节点
        void* node = first;
        int n = 0;
        u64 visited = 0;
        while (node && node != sentinel && visited < declared && n < 256) {
            void* next = nullptr;
            if (!ReadPtr(node, &next)) break;
            void* status = nullptr;
            __try {
                status = *reinterpret_cast<void**>(
                    reinterpret_cast<unsigned char*>(node) + 0x10);
            } __except (EXCEPTION_EXECUTE_HANDLER) { break; }
            if (status) {
                ProcessInstance(status);
                ++n;
            }
            node = next;
            ++visited;
        }
        if (n > 0) {
            total += n;
            Log("[ho] list@save+0x%X: nodes=%d\n", (unsigned)kListCandidates[ci], n);
        }
    }
    g_processedCount = total;
    g_enumerated = true;
}

// ============================================================
// clock 循环
// ============================================================
static void HuntTick() {
    void* base = nullptr;
    {
        const int32_t* disp = reinterpret_cast<const int32_t*>(
            g_exeBase + RVA_CLOCK_LEA + 3);
        uintptr_t addr = g_exeBase + RVA_CLOCK_LEA + 7 + *disp;
        if (!ReadPtr(reinterpret_cast<void*>(addr), &base)) {
            Log("[ho] clock base FAIL\n");
            return;
        }
    }
    unsigned char* save = reinterpret_cast<unsigned char*>(base) + CLOCK_SAVE_0x208;
    u64 rawSecond = 0;
    __try {
        rawSecond = *reinterpret_cast<u64*>(save + SAVE_RAW_SECOND);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[ho] save read FAIL (save=%p)\n", (void*)save);
        return;
    }
    u64 day = rawSecond / 86400;
    if (day != g_lastDay || !g_enumerated) {
        g_lastDay = day;
        Log("[ho] day %lld rescan\n", (long long)day);
        ScanAnimals(save);
    }
}

static DWORD WINAPI HuntLoop(LPVOID) {
    Log("[ho] loop started\n");
    int tick = 0;
    while (g_running) {
        HuntTick();
        if ((++tick % 50) == 0)
            Log("[ho] tick=%d clamped=%d processed=%d vts=%d\n",
                tick, g_totalClamped, g_processedCount, g_vtCount);
        Sleep(100);
    }
    return 0;
}

// ============================================================
// preflight：锚点字节验证
// ============================================================
static bool PreflightAndInit() {
    g_exeBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!g_exeBase) return false;

    const unsigned char* p = reinterpret_cast<const unsigned char*>(g_exeBase);

    // 1. vt_hunt lea 前缀（A0 02 00 00 4C 8D 83 B0 02 00 00 4C 89 44 24 30 48 8D 05）
    static const unsigned char kLeaPre[11] = {
        0xA0, 0x02, 0x00, 0x00, 0x4C, 0x8D, 0x83, 0xB0, 0x02, 0x00, 0x00};
    if (memcmp(p + RVA_VT_HUNT_LEA, kLeaPre, 11) != 0) {
        Log("[ho] FAIL: vt_hunt lea prefix mismatch\n");
        return false;
    }
    // 2. lea 目标验证（+0x10 处 48 8D 05 disp32 -> RVA_VT_HUNT）
    if (p[RVA_VT_HUNT_LEA + 0x10] != 0x48 || p[RVA_VT_HUNT_LEA + 0x11] != 0x8D ||
        p[RVA_VT_HUNT_LEA + 0x12] != 0x05) {
        Log("[ho] FAIL: vt_hunt lea opcode mismatch\n");
        return false;
    }
    {
        const int32_t* disp = reinterpret_cast<const int32_t*>(
            p + RVA_VT_HUNT_LEA + 0x13);
        uintptr_t target = g_exeBase + RVA_VT_HUNT_LEA + 0x17 + *disp;
        if ((target - g_exeBase) != RVA_VT_HUNT) {
            Log("[ho] FAIL: vt_hunt target mismatch (got %p)\n",
                reinterpret_cast<void*>(target - g_exeBase));
            return false;
        }
    }
    // 3. clock lea（48 8B 15 disp32 @ RVA_CLOCK_LEA+0x10）
    if (p[RVA_CLOCK_LEA + 0x10] != 0x48 || p[RVA_CLOCK_LEA + 0x11] != 0x8B ||
        p[RVA_CLOCK_LEA + 0x12] != 0x15) {
        Log("[ho] FAIL: clock lea mismatch\n");
        return false;
    }
    Log("[ho] preflight OK; vt_hunt=%08X\n", (unsigned)RVA_VT_HUNT);
    return true;
}

// ============================================================
// DLL 入口
// ============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        LogOpen("huntoneshot");
    if (!SelfVerifyInit("huntoneshot")) return TRUE;
        if (!PreflightAndInit()) {
            Log("[ho] preflight failed, abort\n");
            LogClose();
            return TRUE;
        }
        g_running = 1;
        CreateThread(nullptr, 0, HuntLoop, nullptr, 0, nullptr);
        Log("[ho] HuntOneShot loaded; base=%p\n", reinterpret_cast<void*>(g_exeBase));
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = 0;
        Log("[ho] unloaded; clamped=%d\n", g_totalClamped);
        LogClose();
    }
    return TRUE;
}