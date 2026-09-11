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

// 使用 memchr 首字节预过滤 + .text 段限定，提升 AOB 扫描速度
static uintptr_t SearchInRange(uint8_t* base, size_t size, const std::vector<uint8_t>& bytes, const std::vector<bool>& mask) {
    if (!base || bytes.empty() || size < bytes.size()) return 0;
    const size_t sigLen = bytes.size();
    // 找第一个需匹配字节作为 memchr 预过滤键
    size_t firstNeed = SIZE_MAX;
    for (size_t j = 0; j < sigLen; ++j) {
        if (mask[j]) { firstNeed = j; break; }
    }
    if (firstNeed == SIZE_MAX) return (uintptr_t)base;  // 全通配
    const uint8_t firstByte = bytes[firstNeed];
    size_t i = firstNeed;
    while (i + sigLen <= size) {
        const uint8_t* hit = (const uint8_t*)memchr(base + i, firstByte, size - i);
        if (!hit) break;
        const size_t pos = (size_t)(hit - base) - firstNeed;
        if (pos + sigLen <= size) {
            bool ok = true;
            for (size_t j = 0; j < sigLen; ++j) {
                if (mask[j] && base[pos + j] != bytes[j]) { ok = false; break; }
            }
            if (ok) return (uintptr_t)(base + pos);
        }
        i = (size_t)(hit - base) + 1;
    }
    return 0;
}

uintptr_t ScanModuleAOB(const char* moduleName, const char* aobText) {
    std::vector<uint8_t> bytes;
    std::vector<bool>    mask;
    if (!ParseAOB(aobText, bytes, mask)) return 0;

    HMODULE hMod = moduleName ? GetModuleHandleA(moduleName) : nullptr;
    if (!hMod) hMod = GetModuleHandleA(nullptr);

    // 只扫描 .text 段（AOB 签名均为代码段函数 prologue）
    uint8_t* scanBase = (uint8_t*)hMod;
    size_t scanSize = 0;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)hMod;
    if (dos && dos->e_magic == IMAGE_DOS_SIGNATURE) {
        PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)((uint8_t*)hMod + dos->e_lfanew);
        if (nt && nt->Signature == IMAGE_NT_SIGNATURE) {
            IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
                if (memcmp(sec[i].Name, ".text", 5) == 0) {
                    scanBase = (uint8_t*)hMod + sec[i].VirtualAddress;
                    scanSize = sec[i].Misc.VirtualSize
                        ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
                    break;
                }
            }
        }
    }
    // 回退：.text 段未找到则扫整个镜像
    if (scanSize == 0) {
        MODULEINFO mi = {0};
        if (!GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) return 0;
        scanBase = (uint8_t*)mi.lpBaseOfDll;
        scanSize = (size_t)mi.SizeOfImage;
    }

    return SearchInRange(scanBase, scanSize, bytes, mask);
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
