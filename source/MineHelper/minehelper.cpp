// minehelper.cpp —— 下矿助手 (MineHelper v1.0.1 for v1.09)
//
// 功能（移植自 MineHelper v1.1.1 by gloaming，1.08.1 → 1.09 重新适配）：
//   - 矿洞中保证最多敲 5 块石头就找到矿洞
//
// 机制（静态分析 minehelper.dll + 1.09 exe 逆向）：
//   游戏"挖矿判定"函数（RVA 0x19EE92 附近）：
//     cmp dword [rdi+0x3C], imm8   <- 该函数是否进入主体逻辑的判定
//     jne +0x349                    <- jne1（跳到 rdi 引用计数收尾 + 返回）
//     cmp byte  [rdi+0x38], 0
//     jne +0x33F                    <- jne2（同样跳到收尾返回）
//     主体路径 = 在矿洞地图上生成对象（分配 0x370 字节 + 坐标计算 + 链表加入）
//   - jne2 恒 NOP×6：主体逻辑每次必走
//   - jne1 保持原样：由 cmp 阈值控制进入主体的频率
//   - cmp 立即数 imm8 动态改写为 dil（上限 5）：按楼层数据 b 调整判定基准，
//     使"进入主体逻辑"的频率与楼层关联，达成"最多 5 块出洞"
//
// 1.09 适配地址：
//   mine_cmp sig   : 0x19E286（唯一）
//   cmp            : RVA 0x19EE92
//   cmp+3 (imm8)   : RVA 0x19EE95
//   jne1           : RVA 0x19EE96
//   jne2           : RVA 0x19EEA0
//   clock 锚点     : RVA 0x19F403（mov rax,[rip+..] 引全局指针），目标 RVA 0x10D4950
//   clock 读链     : [clock] -> +0x208 -> +0x34E0 -> b=[+0x3C], X=[+0x34]
//
// 简化说明（相对原版 1.1.1）：
//   - 不做 Win32 HUD 窗口
//   - 日志走共享框架（发布版关闭）
//   - 去掉 F9 热键 / FirstHole / minehelper.txt 配置
//   - dil 上限 5（原版上限 0x7F=127，约 15 块出洞）
//   - dil 只在值变化时写入（原版每 100ms 无条件写）

#include <windows.h>
#include <cstdint>
#include <atomic>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "logging.h"
#include "selfverify.h"

// 日志开关：诊断版开启
#define MINEHELPER_LOGGING
#ifdef MINEHELPER_LOGGING
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
// 常量（build 25094764 / v1.09）
// ============================================================

// ---- patch 目标 RVA ----
static constexpr uintptr_t RVA_MINE_CMP_IMM = 0x19EE95;  // cmp [rdi+0x3C], imm8 的 imm8
static constexpr uintptr_t RVA_MINE_JNE1    = 0x19EE96;  // jne1（6B：0F 85 rel32）
static constexpr uintptr_t RVA_MINE_JNE2    = 0x19EEA0;  // jne2（6B：0F 85 rel32）
static constexpr uintptr_t RVA_CLOCK_LEA    = 0x19F003;  // mov rax,[rip+..]（引全局指针）

// ---- clock 读链偏移（连续偏移：fb = base + 0x208 + 0x34E0 = base + 0x36E8）----
static constexpr uintptr_t CLOCK_OBJ_0x208    = 0x208;
static constexpr uintptr_t CLOCK_FLOOR_0x34E0 = 0x34E0;  // fb = base+0x208+0x34E0
static constexpr uintptr_t CLOCK_B_OFF        = 0x3C;   // b（楼层/进度）
static constexpr uintptr_t CLOCK_X_OFF        = 0x34;   // X（随机种子）

// ---- 签名（preflight 验证） ----
static const unsigned char kSexpCmpSeq[16] = {
    0x83, 0x7F, 0x3C, 0x00, 0x0F, 0x85, 0x49, 0x03,
    0x00, 0x00, 0x80, 0x7F, 0x38, 0x00, 0x0F, 0x85
};

// ============================================================
// 运行时状态
// ============================================================
static uintptr_t g_exeBase = 0;
static volatile LONG g_running = 0;

// 备份的原始字节
static unsigned char g_origJne1[6];
static unsigned char g_origJne2[6];
static unsigned char g_origCmpImm;

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
    if (v < 0x10000) return false;  // 无效指针
    *out = reinterpret_cast<void*>(v);
    return true;
}

static bool WritePatchBytes(void* addr, const void* data, size_t len) {
    DWORD old = 0;
    if (!VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &old))
        return false;
    memcpy(addr, data, len);
    VirtualProtect(addr, len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), addr, len);
    return true;
}

// 计算 dil（移植自 MineHelper 1.1.1，上限改为 5）
static int ComputeDil(u32 b, u32 x) {
    if (b == 0) return 0;
    const u32 tick = GetTickCount();
    const u32 seed = tick ^ static_cast<u32>(g_exeBase) ^ x;
    int r;
    if (b <= 9) {
        r = static_cast<int>(seed % b) + 1;   // 1..b
    } else {
        r = static_cast<int>(seed % 3);        // 0..2
        r += (b <= 0x1D) ? 5 : 10;             // 5..7 或 10..12
    }
    int dil = static_cast<int>(b) - r;
    if (dil > 5) dil = 5;    // v1.0.1: 上限 5（原版 0x7F=127 约 15 块）
    if (dil < 0) dil = -dil;
    return dil;
}

// ============================================================
// patch 应用
// ============================================================
static void ApplyMineJne2() {
    if (!g_exeBase) return;
    unsigned char nop6[6] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    void* addr = reinterpret_cast<void*>(g_exeBase + RVA_MINE_JNE2);
    if (WritePatchBytes(addr, nop6, 6))
        Log("[mine] jne2 nop6 OK @%p\n", addr);
}

static void ApplyMineJne1(bool on) {
    if (!g_exeBase) return;
    void* addr = reinterpret_cast<void*>(g_exeBase + RVA_MINE_JNE1);
    if (on) {
        unsigned char nop6[6] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        if (WritePatchBytes(addr, nop6, 6))
            Log("[mine] jne1 nop6 ON (FirstHole)\n");
    } else {
        if (WritePatchBytes(addr, g_origJne1, 6))
            Log("[mine] jne1 restored\n");
    }
}

static void ApplyCmpImm(u8 dil) {
    if (!g_exeBase) return;
    void* addr = reinterpret_cast<void*>(g_exeBase + RVA_MINE_CMP_IMM);
    if (WritePatchBytes(addr, &dil, 1))
        Log("[mine] cmp+3 dil=%u\n", dil);
}

// ============================================================
// 主线程：读楼层 -> 计算 dil -> 写 cmp
// ============================================================
static void MineHelperTick() {
    // v1.0.3-diag: clock 读链修正——连续偏移（原版 DLL 语义）
    //   base = [0x10D4950]（全局对象指针）
    //   fb   = base + 0x208 + 0x34E0 = base + 0x36E8（连续加，无中间解引用）
    //   b    = [fb + 0x3C]
    //   x    = [fb + 0x34]
    void* base = nullptr;
    {
        const int32_t* disp = reinterpret_cast<const int32_t*>(
            g_exeBase + RVA_CLOCK_LEA + 3);
        uintptr_t addr = g_exeBase + RVA_CLOCK_LEA + 7 + *disp;
        if (!ReadPtr(reinterpret_cast<void*>(addr), &base)) {
            Log("[mine] base FAIL\n");
            return;
        }
    }
    // fb = base + 0x208 + 0x34E0（连续偏移，原版 DLL 语义）
    unsigned char* fb = reinterpret_cast<unsigned char*>(base) +
                        CLOCK_OBJ_0x208 + CLOCK_FLOOR_0x34E0;
    u32 b = 0, x = 0;
    __try {
        b = *reinterpret_cast<u32*>(fb + CLOCK_B_OFF);
        x = *reinterpret_cast<u32*>(fb + CLOCK_X_OFF);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mine] fb SEH (fb=%p)\n", (void*)fb);
        return;
    }
    Log("[mine] base=%p fb=%p b=%u x=%u\n", base, (void*)fb, b, x);
    // dump fb 附近 0x80 字节（每 4 字节一个 u32）
    __try {
        const u32* p = reinterpret_cast<const u32*>(fb - 0x40);
        for (int row = 0; row < 8; ++row) {
            Log("[mine] fb%+04X: %08X %08X %08X %08X\n", (row - 1) * 0x10,
                p[row * 4 + 0], p[row * 4 + 1], p[row * 4 + 2], p[row * 4 + 3]);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[mine] dump SEH\n");
    }
    if (b == 0 || b >= 10000) return;

    int dil = ComputeDil(b, x);
    if (dil > 5) dil = 5;  // 上限 5
    ApplyCmpImm(static_cast<u8>(dil));
    Log("[mine] applied dil=%d (b=%u x=%u)\n", dil, b, x);
}

static DWORD WINAPI MineHelperLoop(LPVOID) {
    Log("[mine] loop started\n");
    while (g_running) {
        MineHelperTick();
        Sleep(100);
    }
    return 0;
}

// ============================================================
// preflight：验证 RVA 处字节 + 备份原始字节
// ============================================================
static bool PreflightAndBackup() {
    g_exeBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!g_exeBase) return false;

    const unsigned char* p = reinterpret_cast<const unsigned char*>(g_exeBase);

    // 1. 验证 cmp 序列（0x19EE92 处 14 字节）
    if (memcmp(p + RVA_MINE_CMP_IMM - 3, kSexpCmpSeq, 14) != 0) {
        Log("[mine] FAIL: cmp seq mismatch\n");
        return false;
    }
    // 2. 验证 jne1 / jne2 是 0F 85
    if (p[RVA_MINE_JNE1] != 0x0F || p[RVA_MINE_JNE1 + 1] != 0x85 ||
        p[RVA_MINE_JNE2] != 0x0F || p[RVA_MINE_JNE2 + 1] != 0x85) {
        Log("[mine] FAIL: jne sig mismatch\n");
        return false;
    }
    // 3. 验证 clock lea（48 8B 05 disp32）
    if (p[RVA_CLOCK_LEA] != 0x48 || p[RVA_CLOCK_LEA + 1] != 0x8B ||
        p[RVA_CLOCK_LEA + 2] != 0x05) {
        Log("[mine] FAIL: clock lea mismatch\n");
        return false;
    }

    // 备份原始字节
    memcpy(g_origJne1, p + RVA_MINE_JNE1, 6);
    memcpy(g_origJne2, p + RVA_MINE_JNE2, 6);
    g_origCmpImm = p[RVA_MINE_CMP_IMM];
    Log("[mine] backup: jne1=%02X%02X%02X%02X%02X%02X jne2=%02X%02X%02X%02X%02X%02X cmp=%02X\n",
        g_origJne1[0], g_origJne1[1], g_origJne1[2], g_origJne1[3],
        g_origJne1[4], g_origJne1[5],
        g_origJne2[0], g_origJne2[1], g_origJne2[2], g_origJne2[3],
        g_origJne2[4], g_origJne2[5], g_origCmpImm);
    return true;
}

// ============================================================
// DLL 入口
// ============================================================
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        LogOpen("minehelper");
    if (!SelfVerifyInit("minehelper")) return TRUE;
        if (!PreflightAndBackup()) {
            Log("[mine] preflight failed, abort\n");
            LogClose();
            return TRUE;
        }
        g_running = 1;
        ApplyMineJne2();  // jne2 恒 NOP：主体逻辑每次必走
        CreateThread(nullptr, 0, MineHelperLoop, nullptr, 0, nullptr);
        Log("[mine] loaded; base=%p\n", reinterpret_cast<void*>(g_exeBase));
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = 0;
        if (g_exeBase) {
            // 还原所有 patch
            WritePatchBytes(reinterpret_cast<void*>(g_exeBase + RVA_MINE_JNE1),
                            g_origJne1, 6);
            WritePatchBytes(reinterpret_cast<void*>(g_exeBase + RVA_MINE_JNE2),
                           g_origJne2, 6);
            WritePatchBytes(reinterpret_cast<void*>(g_exeBase + RVA_MINE_CMP_IMM),
                           &g_origCmpImm, 1);
        }
        LogClose();
    }
    return TRUE;
}