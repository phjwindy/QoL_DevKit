// ================================================
// 作者：PHJ&消失的清风
// 项目：Village in the Shade QoL MOD Pack
// 转载或分享时请注明出处
// ================================================
// aobscan.cpp
#include "aobscan.h"
#include <windows.h>
#include <psapi.h>
#include <cstring>
#include <sstream>

#pragma comment(lib, "psapi.lib")

namespace qol {

bool ParseAOB(const char* text, std::vector<uint8_t>& out_bytes, std::vector<bool>& out_mask) {
    out_bytes.clear();
    out_mask.clear();
    if (!text) return false;
    std::string s(text);
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        if (tok == "??") {
            out_bytes.push_back(0x00);
            out_mask.push_back(false); // 通配
        } else {
            try {
                uint8_t b = (uint8_t)std::stoi(tok, nullptr, 16);
                out_bytes.push_back(b);
                out_mask.push_back(true);
            } catch (...) {
                return false; // 非法十六进制
            }
        }
    }
    return !out_bytes.empty();
}

static uintptr_t SearchInRange(uint8_t* base, size_t size, const std::vector<uint8_t>& bytes, const std::vector<bool>& mask) {
    if (!base || bytes.empty() || size < bytes.size()) return 0;
    for (size_t i = 0; i + bytes.size() <= size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < bytes.size(); ++j) {
            if (mask[j] && base[i + j] != bytes[j]) { ok = false; break; }
        }
        if (ok) return (uintptr_t)(base + i);
    }
    return 0;
}

uintptr_t ScanModuleAOB(const char* moduleName, const char* aobText) {
    std::vector<uint8_t> bytes;
    std::vector<bool>    mask;
    if (!ParseAOB(aobText, bytes, mask)) return 0;

    HMODULE hMod = moduleName ? GetModuleHandleA(moduleName) : nullptr;
    if (!hMod) hMod = GetModuleHandleA(nullptr); // 无指定则取 exe
    MODULEINFO mi = {0};
    if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) return 0;

    return SearchInRange((uint8_t*)mi.lpBaseOfDll, (size_t)mi.SizeOfImage, bytes, mask);
}

uintptr_t FindPattern(const char* moduleName, const char* aobText) {
    return ScanModuleAOB(moduleName, aobText);
}

uintptr_t ScanProcessAOB(const char* aobText) {
    std::vector<uint8_t> bytes;
    std::vector<bool>    mask;
    if (!ParseAOB(aobText, bytes, mask)) return 0;

    DWORD needed = 0;
    EnumProcessModules(GetCurrentProcess(), nullptr, 0, &needed);
    std::vector<HMODULE> mods(needed / sizeof(HMODULE));
    EnumProcessModules(GetCurrentProcess(), mods.data(), (DWORD)(mods.size() * sizeof(HMODULE)), &needed);

    for (HMODULE h : mods) {
        MODULEINFO mi = {0};
        if (!GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi))) continue;
        uintptr_t r = SearchInRange((uint8_t*)mi.lpBaseOfDll, (size_t)mi.SizeOfImage, bytes, mask);
        if (r) return r;
    }
    return 0;
}

} // namespace qol
