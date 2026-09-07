// hook.h —— x64 trampoline 钩子（自研，不依赖 MinHook）
// 用法：
//   typedef int (__cdecl* TCast)(void);
//   TCast g_orig_CastRod = nullptr;
//   HookInstall((uintptr_t)target, (uintptr_t)&Detour_CastRod, (uintptr_t*)&g_orig_CastRod);
#pragma once
#include <cstdint>

namespace qol {

// 安装钩子：target = 原函数地址, detour = 你的函数, out_orig = 回填的"调用原函数"指针
// 返回 true 成功；失败（内存不可写 / 目标太短）返回 false，调用方应禁用本功能 + 日志 warn
bool HookInstall(uintptr_t target, uintptr_t detour, uintptr_t* out_orig);

// 卸载（可选，用于 unload）
bool HookRemove(uintptr_t target);

// v2: 带原始字节和跳板指针的完整卸载
// savedBytes: HookInstall 前保存的原始 5 字节
// tramp: HookInstall 回填的 out_orig 指针（释放跳板内存）
bool HookRemoveEx(uintptr_t target, const uint8_t* savedBytes, uintptr_t tramp);

} // namespace qol
