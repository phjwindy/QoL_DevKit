// ================================================
// 作者：PHJ&消失的清风
// 项目：Village in the Shade QoL MOD Pack
// 转载或分享时请注明出处
// ================================================
// hook.cpp —— x64 5字节跳转钩子（自研，不依赖 MinHook）
// 采用"跳板页 + 保存原始5字节 + jmp 回 target+5"的安全模型：
//   *out_orig 指向跳板：先执行原函数前5字节，再跳回 target+5，可多次调用原函数。
// 注意：需要 target 前 5 字节长度内不存在跨指令边界问题，且原函数可安全重入。
// v2: trampoline RIP-relative 重定位 + HookRemoveEx 带字节还原。
#include "hook.h"
#include <windows.h>
#include <cstring>
#include <cstdint>

namespace qol {

// 计算 E9 rel32 位移并做 ±2GB 溢出检查（返回 false 表示溢出，禁止静默截断）
static bool MakeRel32(uintptr_t from, uintptr_t to, int32_t* out) {
    intptr_t delta = (intptr_t)(to - from);
    if (delta < INT32_MIN || delta > INT32_MAX) return false;
    *out = (int32_t)delta;
    return true;
}

#pragma pack(push, 1)
struct JmpAbs {
    uint8_t   mov_rax[2] = {0x48, 0xB8}; // mov rax, imm64
    uintptr_t imm64      = 0;
    uint8_t   jmp_rax[2] = {0xFF, 0xE0}; // jmp rax
};
struct JmpRel32 {
    uint8_t opcode = 0xE9;
    int32_t rel32  = 0;
};
#pragma pack(pop)

// ---- RIP-relative 指令检测与重定位 ----
// 检查 5 字节窗口内是否存在 RIP-relative 指令（ModRM 含 [rip+disp32]）。
// 如果存在，修正 disp32 使得重定位后仍指向原绝对地址。
// 返回 true 表示已处理（或无需处理），false 表示检测到不可重定位指令。
static bool RelocateRipRelative(uint8_t* code, uintptr_t oldRip, uintptr_t newRip) {
    // x64 RIP-relative 指令的特征：ModRM 字节的 mod=00, rm=101（即 [rip+disp32]）
    // 常见模式：8B/89/0F(8x)/FF(15) 等，后跟 4 字节 disp32
    // 简化策略：扫描 5 字节窗口内的每个可能位置，查找以 disp32 结尾的 RIP-relative 编码
    for (int i = 0; i < 5; ++i) {
        uint8_t b = code[i];
        // 单字节指令不可能是 RIP-relative 前缀，跳过常见前缀
        if (b == 0x40 || b == 0x41 || b == 0x42 || b == 0x43 ||
            b == 0x44 || b == 0x45 || b == 0x46 || b == 0x47 ||
            b == 0x48 || b == 0x49 || b == 0x4C || b == 0x4D ||
            b == 0x66 || b == 0x67 || b == 0xF0 || b == 0xF2 || b == 0xF3) {
            continue;
        }
        // 检查是否是带 ModRM 的操作码（需要至少 op+modrm+disp32 = 6 字节，
        // 但我们的窗口只有 5 字节，所以只在 i<=1 时才可能有完整 disp32）
        if (i > 1) break;  // disp32 需要 4 字节空间，op+modrm=2 字节，最多从 i=0 或 i=1 开始

        // 尝试解码：op (可选 REX prefix) + ModRM
        int modrmIdx = i;
        if (b >= 0x40 && b <= 0x4F) {
            // REX prefix，ModRM 在下一位
            if (i + 1 >= 5) continue;
            modrmIdx = i + 1;
        }

        // 检查 ModRM 字节是否指示 [rip+disp32]（mod=00, rm=101）
        uint8_t modrm = code[modrmIdx];
        uint8_t mod = (modrm >> 6) & 3;
        uint8_t rm = modrm & 7;
        if (mod == 0 && rm == 5) {
            // [rip+disp32]：disp32 从 modrmIdx+1 开始
            int disp32Idx = modrmIdx + 1;
            if (disp32Idx + 4 > 5) {
                // disp32 超出 5 字节窗口——指令跨越边界，无法安全重定位
                return false;
            }
            int32_t oldDisp;
            memcpy(&oldDisp, code + disp32Idx, 4);
            // 原指令的 RIP = oldRip + (disp32Idx + 4)（指令执行完后的 PC）
            // 原目标地址 = oldRip + (disp32Idx + 4) + oldDisp
            // 新 RIP = newRip + (disp32Idx + 4)
            // newDisp = 原目标地址 - 新 RIP = oldRip + (disp32Idx + 4) + oldDisp - (newRip + (disp32Idx + 4))
            //         = oldRip + oldDisp - newRip
            int64_t newDisp = (int64_t)oldDisp + (int64_t)oldRip - (int64_t)newRip;
            // 检查溢出
            if (newDisp < INT32_MIN || newDisp > INT32_MAX) return false;
            int32_t newDisp32 = (int32_t)newDisp;
            memcpy(code + disp32Idx, &newDisp32, 4);
            // 只处理第一处 RIP-relative（5 字节窗口内通常最多一处）
            return true;
        }
    }
    return true;  // 无 RIP-relative 指令，无需修正
}

bool HookInstall(uintptr_t target, uintptr_t detour, uintptr_t* out_orig) {
    if (!target || !detour || !out_orig) return false;

    // 1) 分配可执行跳板页
    JmpAbs* tramp = (JmpAbs*)VirtualAlloc(nullptr, sizeof(JmpAbs),
                                          MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tramp) return false;

    // 2) 跳板 = 原函数前 5 字节 + jmp target+5（用于安全调用原函数）
    uint8_t* tbytes = (uint8_t*)tramp;
    memcpy(tbytes, (void*)target, 5);

    // v2: 重定位 RIP-relative 指令（如果有）
    // oldRip = target（原指令地址），newRip = (uintptr_t)tramp（跳板地址）
    if (!RelocateRipRelative(tbytes, target, (uintptr_t)tramp)) {
        VirtualFree(tramp, 0, MEM_RELEASE);
        return false;
    }

    JmpRel32* back = (JmpRel32*)(tbytes + 5);
    // 跳板回跳：rel32 溢出则释放跳板页并失败（避免生成错误地址）
    if (!MakeRel32((uintptr_t)(back + 1), target + 5, &back->rel32)) {
        VirtualFree(tramp, 0, MEM_RELEASE);
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(), tramp, sizeof(JmpAbs));

    // 3) 改写 target 前 5 字节为 jmp detour（用绝对跳板更稳妥：mov rax,detour; jmp rax）
    //    这里用 0xE9 rel32 直接跳（detour 与 target 距离须在 ±2GB 内，通常满足）
    DWORD old = 0;
    if (!VirtualProtect((void*)target, 5, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(tramp, 0, MEM_RELEASE);
        return false;
    }
    JmpRel32 patch;
    // 主 hook 跳转：detour(DLL) 与 target(exe) 距离可能超 ±2GB，溢出则失败而非截断
    if (!MakeRel32(target + 5, detour, &patch.rel32)) {
        VirtualProtect((void*)target, 5, old, &old); // 还原页权限后释放跳板
        VirtualFree(tramp, 0, MEM_RELEASE);
        return false;
    }
    memcpy((void*)target, &patch, 5);
    VirtualProtect((void*)target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)target, 5);

    *out_orig = (uintptr_t)tramp; // 调用方通过此指针调用原函数
    return true;
}

// v2: 带原始字节和跳板指针的完整卸载
bool HookRemoveEx(uintptr_t target, const uint8_t* savedBytes, uintptr_t tramp) {
    if (!target || !savedBytes) return false;
    DWORD old = 0;
    if (!VirtualProtect((void*)target, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy((void*)target, savedBytes, 5);
    VirtualProtect((void*)target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)target, 5);
    if (tramp) VirtualFree((void*)tramp, 0, MEM_RELEASE);
    return true;
}

bool HookRemove(uintptr_t target) {
    // 骨架版不实现运行时还原（插件卸载多随进程退出）。
    // 如需还原，请使用 HookRemoveEx(target, savedBytes, tramp)。
    (void)target;
    return false;
}

} // namespace qol