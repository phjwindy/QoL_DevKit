// plugin.cpp —— 插件模板（★复制此文件 = 一个新插件，只需改 3 处标记 ★）
//
// ★改 ①：功能名（日志/DLL 名，全小写）
// ★改 ②：AOB 特征码（DEV_GUIDE §6，从 BigL233 .inl 提取 24646798，由用户补 24969282）
// ★改 ③：Tick 逻辑（每帧 / 输入触发）

#include <windows.h>
#include "logging.h"
#include "aobscan.h"
#include "hook.h"
#include "feature.h"

namespace {

// ★改 ②：本功能需要的 Hook 点（24969282 真实字节由用户提供，先用旧版占位）
struct Offsets {
    const char* get_nearby_chest = "48 89 5C 24 18 55 56 57 ?? ?? 48 83 EC 28"; // 示意
    const char* sort_items      = "40 53 48 83 EC 20 48 8B D9 ?? ?? 48 8B 0D";
} g_offs;

// 原始函数指针（Hook 回填）
using TGetNearbyChest = uintptr_t(__cdecl*)();
using TSortItems       = void(__cdecl*)();
TGetNearbyChest g_orig_GetNearbyChest = nullptr;
TSortItems       g_orig_SortItems       = nullptr;

// ★改 ③：Detour（你的逻辑插入点）
uintptr_t Detour_GetNearbyChest() {
    // TODO: 插入自定义逻辑（如范围扩大 / 调试打印）
    return g_orig_GetNearbyChest();
}
void Detour_SortItems() {
    qol::Log("[chestsort] SortItems called");
    g_orig_SortItems();
}

// ★功能类（崩溃隔离 + 状态）
class ChestSortFeature : public qol::Feature {
public:
    ChestSortFeature() : Feature("chestsort") {}

    bool Init() override {
        // 1) 定位函数（DEV_GUIDE §6：按当前 build 选 AOB；此处先用占位，缺省时禁用）
        uintptr_t a = qol::ScanModuleAOB("village.exe", g_offs.get_nearby_chest);
        uintptr_t b = qol::ScanModuleAOB("village.exe", g_offs.sort_items);
        if (!a || !b) {
            qol::Log("[chestsort] AOB not found (build 24969282) — feature disabled");
            return false; // ★失败 → 禁用本功能，不崩溃
        }

        // 2) 装 Hook（失败同样禁用）
        if (!qol::HookInstall(a, (uintptr_t)&Detour_GetNearbyChest, (uintptr_t*)&g_orig_GetNearbyChest)) {
            qol::Log("[chestsort] HookInstall failed (GetNearbyChest)");
            return false;
        }
        if (!qol::HookInstall(b, (uintptr_t)&Detour_SortItems, (uintptr_t*)&g_orig_SortItems)) {
            qol::Log("[chestsort] HookInstall failed (SortItems)");
            return false;
        }
        return true;
    }

    void Tick() override {
        if (!Enabled() || !Ready()) return;
        // ★改 ③：每帧逻辑 / 检测十字键上·F8 触发归类
        // TODO: 检测输入 → 调用 g_orig_SortItems() 或自定义归类
    }
};

ChestSortFeature g_feature;

} // namespace

// ===== 插件入口（加载器要求：mod_init / mod_tick / unload）=====

extern "C" __declspec(dllexport) void mod_init(void) {
    qol::Log("[chestsort] mod_init() called");
    // ★核心初始化放在 DllMain 兜底；此处可放基座驱动的二次初始化
}

extern "C" __declspec(dllexport) void mod_tick(void) {
    g_feature.SafeTick();
}

extern "C" __declspec(dllexport) void unload(void) {
    qol::Log("[chestsort] unload() called");
    // TODO: 清理资源、卸载 Hook、停止线程等
}

BOOL APIENTRY DllMain(HMODULE /*h*/, DWORD reason, LPVOID /*lpReserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        qol::SetLogName("chestsort");
        qol::Log("[chestsort] DllMain attach — 1.08.1 build 24969282");
        g_feature.SafeInit(); // ★★ 初始化在这里（基座不调用 init/tick 时的兜底）
    } else if (reason == DLL_PROCESS_DETACH) {
        qol::Log("[chestsort] detach");
    }
    return TRUE;
}
