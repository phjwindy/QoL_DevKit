// aobscan.h —— AOB 特征码扫描（支持 '??' 通配符）
// 用法：ScanAOB("village.exe", "48 89 5C 24 18 55 56 57 ?? ?? 48 83 EC 28");
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace qol {

// 解析 "AA BB CC ?? DD" 为字节 + 掩码（mask=true 表示需匹配）
bool ParseAOB(const char* text, std::vector<uint8_t>& out_bytes, std::vector<bool>& out_mask);

// 在指定模块内扫描，返回第一个匹配 VA（找不到返回 0）
uintptr_t ScanModuleAOB(const char* moduleName, const char* aobText);

// 扫描整个进程（慎用，慢）—— 一般 ScanModuleAOB 足够
uintptr_t ScanProcessAOB(const char* aobText);

// 简易特征码表：按 build 号查表（DEV_GUIDE §6）
struct AOBEntry {
    const char* name;
    const char* sig_24646798; // BigL233 原始（参考/占位）
    const char* sig_24969282; // ★1.08.1 真实值，由用户提供
};

// 便捷函数：从模块内扫描 AOB 字符串并返回地址；失败返回 0
uintptr_t FindPattern(const char* moduleName, const char* aobText);

} // namespace qol

// ===== 全局兼容别名（旧骨架 chestsort.cpp 以无命名空间方式调用）=====
// 扫描主模块（village.exe / 本进程 exe）
static inline uintptr_t ScanAOB(const char* /*module*/, const char* aobText) {
    return qol::ScanModuleAOB(nullptr, aobText);
}
