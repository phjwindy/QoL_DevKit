// memory_cache.h —— 共享内存可读检查 + FastRegion 区域缓存
//
// v1.1 (2026-09-08): thread_local + 32 槽 + round-robin 淘汰（替代满了就丢）。
// v1.0 (2026-09-08): 从 SickleHarvest/AutoPet/ChestSort/AutoHarvest 四份
//   独立实现中提取，统一为共享版本。
//
// 用法：
//   #include "memory_cache.h"
//   // 在需要批量扫描的函数开头重置缓存：
//   qol_mem::FastRegionReset();
//   // 然后正常调用 qol_mem::IsReadable / qol_mem::ReadPtr
//
// 作者：PHJ&消失的清风，转载或分享时请注明出处。

#pragma once
#include <windows.h>
#include <cstdint>

namespace qol_mem {

struct MemRegion {
    uintptr_t start;
    uintptr_t end;
};

constexpr int FAST_REGION_SLOTS = 32;  // v1.1: 16→32 槽

// v1.1: thread_local 确保多线程安全（与 Sower/ProductionAuto 的 thread_local 对齐）
inline thread_local MemRegion g_fastRegions[FAST_REGION_SLOTS];
inline thread_local int g_fastRegionCount = 0;
inline thread_local int g_fastRegionReplace = 0;  // v1.1: round-robin 替换游标

inline void FastRegionReset() {
    g_fastRegionCount = 0;
    g_fastRegionReplace = 0;
}

inline bool IsReadable(const void* pointer, size_t size) {
    if (!pointer || size == 0) return false;
    uintptr_t start = reinterpret_cast<uintptr_t>(pointer);
    if (start < 0x10000 || start > 0x00007FFFFFFFFFFFULL) return false;
    // 命中缓存区域
    for (int i = 0; i < g_fastRegionCount; ++i) {
        if (start >= g_fastRegions[i].start &&
            start + size <= g_fastRegions[i].end) return true;
    }
    // 未命中 → 真实 VirtualQuery
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(pointer, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
    uintptr_t regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    uintptr_t regionEnd = regionStart + mbi.RegionSize;
    if (start + size < start || start + size > regionEnd) return false;
    // v1.1: round-robin 替换（满了后覆盖最旧条目，而非丢弃）
    if (g_fastRegionCount < FAST_REGION_SLOTS) {
        g_fastRegions[g_fastRegionCount] = { regionStart, regionEnd };
        g_fastRegionCount++;
    } else {
        g_fastRegions[g_fastRegionReplace] = { regionStart, regionEnd };
        g_fastRegionReplace = (g_fastRegionReplace + 1) % FAST_REGION_SLOTS;
    }
    return true;
}

inline bool ReadPtr(const void* obj, uintptr_t off, void** out) {
    if (!obj || !out) return false;
    const void* field = reinterpret_cast<const void*>(
        reinterpret_cast<uintptr_t>(obj) + off);
    if (!IsReadable(field, sizeof(void*))) return false;
    *out = *reinterpret_cast<void* const*>(field);
    return *out != nullptr;
}

} // namespace qol_mem
