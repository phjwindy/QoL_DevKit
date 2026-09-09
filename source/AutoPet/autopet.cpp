// autopet.cpp —— 自动抚摸动物（AutoPet v0.5.2）
//
// v0.5.1: 改为跨日触发——读 save+0x3270 (raw_second) 判断天数变化，
//         睡觉跨日后 caress 被原生重置为 0，此时执行一次性抚摸。
//         同一天内不重复扫描。进游戏后延迟 10 秒首次扫描。
// v0.5.0: HUD 左中显示已抚摸/总数 + 扫描周期优化。
// v0.4.9: FEATURE 过滤修正（&0x03 而非 !=0），狗子终于可摸。
// v0.4.8: 去掉距离过滤，保留 LIVE_VALID + FEATURE + caress 过滤。
// v0.4.6: 改用 livestockList_ 链表遍历（非空间搜索），覆盖牛/马/狗等所有动物。
// v0.4.6: 改用 livestockList_ 链表遍历（非空间搜索），覆盖牛/马/狗等所有动物。——打印空间搜索结果里所有对象的
//             vtable，确认狗是否在空间索引内、其 vtable 是什么，为支持
//             狗（动物类型 200，参考实现 RVA_AUTO_PET_LIVESTOCK_VTABLE
//             =0xE047A8 vs 我们的 0xE07850）提供依据。
//
// v0.4.3: 性能优化——v0.4.2 DirectRead 实测反劣（搜索 58~85ms），根因是
//         每次字段读都触发 IsReadable→VirtualQuery，比 RPM 还多一次系统调用。
//         改为移植 SickleHarvest 已验证的 FastRegion 区域缓存（16 槽
//         MemRegion，VirtualQuery 结果缓存命中），每轮扫描前 FastRegionReset()，
//         200+ 次字段读仅 ~16 次 VirtualQuery，目标降到 ~1ms。
//         （v0.4.3 实测：搜索 8~17ms，掉帧已明显缓解；但狗子未被扫描到）
//
// v0.4.2: 性能优化——热路径 RPM 读取（ReadProcessMemory）改为 DirectRead
//         （IsReadable 校验 + 直接解引用），消除每 2 秒扫描 ~50ms 卡顿
//         （v0.4.1-diag 实测：扫描 39~54ms/次，RPM 系统调用为主因）
//
// v0.4.1-diag: FPS 掉帧排查诊断版——扫描/结算阶段帧内耗时计时日志
//
// v0.4.0 正式版（相对 v0.3.3 诊断版）：
//   - 移除诊断刷屏日志（rawCount/candidate 每秒 4~5 条），仅保留 [Pet] settled 关键行
//   - 降低空间搜索频率（60 tick → 120 tick，约 2 秒一次），减轻每帧开销
//   - 核心逻辑不变（v0.3.3 已实测：4 只家畜 love +6、caress 每日一次、不碰 foodNum_）
//
// 功能：靠近家畜时自动抚摸，每日每只限一次。
// 设计原则：
//   - 纯位置驱动，无面板
//   - 结算走原生函数（love setter + caress key-value），不触碰游戏进食/喂养状态位
//   - 规避第三方 AutoPet v0.2.5 "抚摸后进食状态也被标记完成"的 bug
//
// v0.3.0 正式版（相对 v0.2.0-diag 的改动）：
//   1. 接入 3 个原生函数 RVA（经 AOB 扫描 + 脚本锚点交叉验证）：
//      - LOVE_SETTER   = 0x2928D0  （被 ScriptCommand::LivestockSetLoveRate/GetLoveRate 调用）
//      - RATE_MODIFIER = 0x130FE0  （被 ScriptCommand::LivestockGetLoveRate 调用）
//      - STATUS_SETTER = 0x14E9D0  （map 写入语义：写 "caress"=1 每日标记）
//   2. 靠近检测（距离 <= PET_RADIUS 220.0f）→ 原生结算好感 + 每日标记
//   3. 不触碰 foodNum_/进食状态位，每日重置由原生日落清 caress
//
// 逆向基础（v1.09 build 25094764, ImageBase=0x140000000）：
//   - CLivestockStatus 主 vtable RVA=0xE07850（24 槽，已验证）
//   - 空间搜索模板 + 回调 vtable 0xE1C210（ChestSort 同款，已验证）
//   - LoveSetter 经脚本命令 LivestockSetLoveRate(0x491BF7) 交叉验证
//
// 参考实现：Village_QoL_Mod-main（BigL233 开源）source\auto_pet.inl
//   字段偏移：AUTO_PET_STATUS_* 系列（0xE0/0x150/0x158/0x240/0x290/0x2C0/0x300/0x328/0x330）
//   caress key：AUTO_PET_CARESS_KEY = 0x0000737365726163ULL（ASCII "caress"）
//   结算链（v0.3.0 已接入）：
//     rateModifier(playerStatus, 11) → (mod+100)*5/100
//     loveSetter(status, love+delta, false)
//     statusSetter(status+0x10, &caressKey, 1)
//
// 作者：PHJ&消失的清风，转载或分享时请注明出处。

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <unordered_set>
#include <vector>
#include <cmath>

#include "logging.h"
#include "aobscan.h"
#include "hook.h"
#include "feature.h"
#include "hotkey.h"
#include "selfverify.h"

// ============================================================
// 日志开关（正式版保留关键日志，方便实测验证）
// ============================================================
#define AUTOPET_LOGGING
#ifdef AUTOPET_LOGGING
  // 使用共享框架日志
#else
  #define LogOpen(x)  ((void)0)
  #define Log(...)    ((void)0)
  #define LogV(...)   ((void)0)
  #define LogClose()  ((void)0)
#endif

// ============================================================
// 版本常量（build 25094764 / v1.09）
// ============================================================
static constexpr const char* AUTOPET_VERSION = "v0.5.2";

// ImageBase（PE 默认）
static constexpr uintptr_t IMAGE_BASE = 0x140000000;

// 逆向定位的 RVA（build 25094764）
namespace rva {
    // CLivestockStatus vtable（主继承层级，有最多槽位）
    static constexpr uintptr_t LIVESTOCK_VTABLE = 0xE07850;
    // CCreatureStatus vtable
    static constexpr uintptr_t CREATURE_VTABLE = 0xE05618;
    // CCom_Unit_Livestock vtable
    static constexpr uintptr_t COM_UNIT_LIVESTOCK_VTABLE = 0xE14D68;
    // MapSearchStatusInRectTemplate<CLivestockStatus> 回调 vtable
    // （_Func_impl_no_alloc<CLivestockStatus>，RTTI 链定位）
    static constexpr uintptr_t SEARCH_CALLBACK_VTABLE = 0xE1C210;
    // 回调 vtable 槽位（与 ChestSort 一致布局）
    static constexpr uintptr_t SEARCH_CALLBACK_COPY    = 0x280F00;  // vt[0]
    static constexpr uintptr_t SEARCH_CALLBACK_INVOKE  = 0x280E90;  // vt[3] 或 vt[2]
    static constexpr uintptr_t SEARCH_CALLBACK_DESTROY = 0x18C290;  // vt[4]（与 ChestSort 相同，验证过）
    // 'caress' 脚本键字符串
    static constexpr uintptr_t STR_CARESS = 0xE07188;

    // ---- 世界上下文（复用 ChestSort 链路） ----
    static constexpr uintptr_t GAME_ROOT         = 0x10D4950;  // 全局根指针
    static constexpr uintptr_t ROOT_PLAYER_OFFSET = 0x208;
    static constexpr uintptr_t ROOT_MAP_OWNER_OFFSET = 0x268;
    static constexpr uintptr_t MAP_INFO_OFFSET    = 0x0d8;
    static constexpr uintptr_t MAP_SPATIAL_OWNER_OFFSET = 0x030;
    static constexpr uintptr_t MAP_SPATIAL_INDEX_OFFSET = 0x6e0;
    static constexpr uintptr_t PLAYER_OBJECT_STATUS_OFFSET = 0x32b8;
    static constexpr uintptr_t OBJECT_POSITION_OFFSET = 0x0f0;

    // v0.4.6: livestockList_ 链表偏移（CSaveData + 0x3430）
    // 参考实现 BigL233 auto_pet.inl L147: AUTO_PET_LIVESTOCK_LIST_OFFSET = 0x3430
    // save = root+0x208（与 ChestSort 的 player 同一指针，因为 player = save 对象）
    static constexpr uintptr_t LIVESTOCK_LIST_OFFSET = 0x3430;
    static constexpr size_t MAX_LIVESTOCK_LIST = 256;  // 链表节点上限

    // v0.5.1: 游戏时间（save+0x3270 = raw_second，int64）
    // dayId = rawSecond / 86400，跨日时 caress 被原生重置为 0
    static constexpr uintptr_t RAW_SECOND_OFFSET = 0x3270;
    static constexpr int64_t SECONDS_PER_DAY = 86400;

    // ---- v0.3.0 新增：原生结算函数（AOB + 脚本锚点交叉验证） ----
    // LOVE_SETTER：ScriptCommand::LivestockSetLoveRate 交叉验证（0x491CAB -> 0x2928D0）
    static constexpr uintptr_t LOVE_SETTER   = 0x2928D0;
    // RATE_MODIFIER：ScriptCommand::LivestockGetLoveRate 交叉验证（0x491B2E -> 0x130FE0）
    static constexpr uintptr_t RATE_MODIFIER = 0x130FE0;
    // STATUS_SETTER：map 写入语义（写 status+0x10 的 "caress" 键），16 字节签名 + prologue 验证
    static constexpr uintptr_t STATUS_SETTER = 0x14E9D0;
}

// ============================================================
// BigL233 真实字段偏移（build 24646798，经 v0.1.3 日志部分验证）
// ============================================================
namespace field {
    // CLivestockStatus 内嵌 map 的基址偏移（"caress" key-value 序列化表）
    static constexpr uintptr_t CA_MAP_BASE     = 0x10;   // status+0x10 = map 对象
    static constexpr uintptr_t MAP_SENTINEL    = 0x10;   // map+0x10 = sentinel 指针
    static constexpr uintptr_t MAP_SIZE        = 0x18;   // map+0x18 = size (u64)
    static constexpr uintptr_t MAP_BUCKETS     = 0x20;   // map+0x20 = bucket 向量
    static constexpr uintptr_t MAP_MASK        = 0x38;   // map+0x38 = mask (u64)

    static constexpr uintptr_t UNIQUE_ID       = 0x0E0;  // 唯一 ID（u64）
    static constexpr uintptr_t LIVE_OBJECT     = 0x150;  // 场景实时对象指针
    static constexpr uintptr_t LIVE_VALID      = 0x158;  // 实时对象有效标记（byte*）
    static constexpr uintptr_t GIMMICK_HOLDER  = 0x240;  // GimmickData holder 指针
    static constexpr uintptr_t ANIMAL_HOLDER   = 0x290;  // 动物 holder 指针
    static constexpr uintptr_t PLACEMENT_HOLDER= 0x2C0;  // 放置 holder 指针
    static constexpr uintptr_t LOVE            = 0x300;  // 好感度（int）
    static constexpr uintptr_t FEATURE_LOW     = 0x328;  // 特征低位（u8）
    static constexpr uintptr_t FEATURE_HIGH    = 0x330;  // 特征高位（u8）
}

// "caress" key：ASCII 小端 = 0x0000737365726163
static constexpr uint64_t CARESS_KEY = 0x0000737365726163ULL;

// ============================================================
// AOB 签名（复用 ChestSort）
// ============================================================
struct AOBDef {
    const char* name;
    const char* hex;
};
static const AOBDef kAOBs[] = {
    {"spatial_search",   "40 57 41 54 41 55 41 56 41 57 48 83 EC 60"},
    {"raw_vector_free",  "48 83 EC 38 48 81 FA 00 10 00 00 72 14 48 8B"},
};
enum AOBIndex { IDX_SPATIAL_SEARCH = 0, IDX_RAW_VECTOR_FREE = 1 };
static constexpr size_t kAOBCount = sizeof(kAOBs) / sizeof(kAOBs[0]);

// ============================================================
// 全局状态
// ============================================================
namespace G {
    uintptr_t base = 0;                    // village.exe 模块基址
    bool ready = false;                    // 初始化完成
    bool vtableVerified = false;           // vtable 地址验证通过

    // vtable 绝对地址（运行时计算）
    uintptr_t livestockVtable = 0;
    uintptr_t creatureVtable = 0;
    uintptr_t comUnitLivestockVtable = 0;

    // 空间搜索函数（AOB 扫描）
    using FnSpatialSearch = bool(__fastcall*)(void*, const void*, void*, int, int);
    using FnRawVectorFree = void(__fastcall*)(void*, size_t);
    FnSpatialSearch spatialSearch = nullptr;
    FnRawVectorFree rawVectorFree = nullptr;

    // 回调 vtable 绝对地址
    uintptr_t callbackVtable = 0;

    // 扫描节流
    std::atomic<uint64_t> lastScanTick{0};
    static constexpr uint64_t SCAN_INTERVAL_TICKS = 300; // ~5秒@60fps，检查天数变化用
    int64_t lastDayId = -1;  // v0.5.1: 上次扫描的游戏天数，-1=首次
    int64_t firstScanDelayTicks = 0;  // v0.5.1: 进游戏后延迟首次扫描（等场景加载）

    // ---- v0.3.0 原生结算函数 ----
    // int64_t (__fastcall*)(void* playerStatus, int mode)
    using FnRateModifier = int64_t(__fastcall*)(void*, int);
    // void (__fastcall*)(void* status, int love, bool flag)
    using FnLoveSetter = void(__fastcall*)(void*, int, bool);
    // void (__fastcall*)(void* mapBase, const uint64_t* key, int value)
    using FnStatusSetter = void(__fastcall*)(void*, const uint64_t*, int);
    FnRateModifier rateModifier = nullptr;
    FnLoveSetter loveSetter = nullptr;
    FnStatusSetter statusSetter = nullptr;
}

// ============================================================
// 安全内存读取（只读诊断，不写入任何游戏内存）
// ============================================================
static bool SafeRead(void* addr, void* buf, size_t len) {
    if (!addr || !buf || len == 0) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(GetCurrentProcess(), addr, buf, len, &bytesRead)
           && bytesRead == len;
}

// 读取指针（验证可读）
static uintptr_t SafeReadPtr(void* addr) {
    uintptr_t val = 0;
    if (SafeRead(addr, &val, sizeof(val)))
        return val;
    return 0;
}

// ============================================================
// FastRegion 区域缓存（移植 SickleHarvest 已验证方案）
// 同区域多次访问只查一次系统调用。v0.4.2 实测每次字段读
// 都触发 VirtualQuery 反而比 RPM 慢；缓存后 200+ 次字段读
// 仅 ~16 次 VirtualQuery。
// ============================================================
struct MemRegion {
    uintptr_t start;
    uintptr_t end;
};
static MemRegion g_fastRegions[16];
static int g_fastRegionCount = 0;

// 每轮扫描开始前必须调用（清空缓存，防止跨帧区域失效）
static void FastRegionReset() {
    g_fastRegionCount = 0;
}

// 内存可读性检查（VirtualQuery + 16 槽区域缓存）
static bool IsReadable(const void* addr, size_t len) {
    if (!addr || len == 0) return false;
    uintptr_t start = reinterpret_cast<uintptr_t>(addr);
    if (start < 0x10000 || start > 0x00007FFFFFFFFFFFULL) return false;
    // 命中缓存区域
    for (int i = 0; i < g_fastRegionCount; ++i) {
        if (start >= g_fastRegions[i].start &&
            start + len <= g_fastRegions[i].end) return true;
    }
    // 未命中 → 真实 VirtualQuery
    MEMORY_BASIC_INFORMATION mbi = {};
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
    uintptr_t regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    uintptr_t regionEnd = regionStart + mbi.RegionSize;
    if (start + len < start || start + len > regionEnd) return false;
    // 记录该区域
    if (g_fastRegionCount < 16) {
        g_fastRegions[g_fastRegionCount] = { regionStart, regionEnd };
        g_fastRegionCount++;
    }
    return true;
}

// ============================================================
// 直接内存读取（热路径优化：FastRegion IsReadable 校验 + 直接解引用）
// 同进程内不必要用 ReadProcessMemory（每次跨内核态~2us），
// 直接读内存快 ~100 倍。读取前 VirtualQuery 保证安全，
// 且 v0.4.3 起 IsReadable 走 16 槽区域缓存，仅首访触发系统调用。
// ============================================================
static bool DirectRead(const void* addr, void* buf, size_t len) {
    if (!addr || !buf || len == 0) return false;
    if (!IsReadable(addr, len)) return false;
    memcpy(buf, addr, len);
    return true;
}

// 读取指针字段
static bool ReadPtr(const void* obj, uintptr_t off, void** out) {
    if (!obj || !out) return false;
    const void* field = (const void*)((uintptr_t)obj + off);
    if (!IsReadable(field, sizeof(void*))) return false;
    *out = *(void* const*)field;
    return *out != nullptr;
}

// ============================================================
// vtable 地址验证：读取 vtable[-1] 处的 COL，检查 signature=1
// ============================================================
static bool VerifyVtable(uintptr_t vtableRVA, const char* name) {
    if (!G::base) return false;
    uintptr_t vtAddr = G::base + vtableRVA;

    // vtable[-1] = 指向 COL 的指针
    uintptr_t colAbs = SafeReadPtr(reinterpret_cast<void*>(vtAddr - 8));
    if (!colAbs) {
        Log("[autopet] %s vtable[-1] read FAILED at 0x%llx", name,
            static_cast<unsigned long long>(vtAddr - 8));
        return false;
    }

    // COL signature = 1 (x64)
    uint32_t sig = 0;
    if (!SafeRead(reinterpret_cast<void*>(colAbs), &sig, sizeof(sig))) {
        Log("[autopet] %s COL read FAILED at 0x%llx", name,
            static_cast<unsigned long long>(colAbs));
        return false;
    }
    if (sig != 1) {
        Log("[autopet] %s COL sig=%u (expected 1) at 0x%llx", name, sig,
            static_cast<unsigned long long>(colAbs));
        return false;
    }

    Log("[autopet] %s vtable OK: vt=0x%llx COL=0x%llx sig=1",
        name, static_cast<unsigned long long>(vtAddr),
        static_cast<unsigned long long>(colAbs));
    return true;
}

// ============================================================
// 空间搜索（复用 ChestSort 成熟方案）
// ============================================================
struct Rect { float minX, minY, maxX, maxY; };

struct RawPointerVector { void** begin; void** end; void** capacity; };

// 搜索回调（仿函数布局，0x40 字节）
struct SearchCallback {
    alignas(16) unsigned char storage[0x38];
    void* target;
};
static_assert(sizeof(SearchCallback) == 0x40, "callback size");

static void ReleaseSearchVector(RawPointerVector* v) {
    if (!v || !v->begin || !G::rawVectorFree) return;
    uintptr_t b = (uintptr_t)v->begin, c = (uintptr_t)v->capacity;
    size_t allocBytes = c >= b ? c - b : 0;
    if (allocBytes % sizeof(void*) == 0 && allocBytes <= 16384 * sizeof(void*)) {
        G::rawVectorFree(v->begin, allocBytes);
    }
    *v = {};
}

// 执行一次空间搜索，返回原始结果
static bool DoSpatialSearch(void* spatialIndex, const Rect* bounds, uint64_t filterId,
                            RawPointerVector* results) {
    if (!spatialIndex || !bounds || !results) return false;
    memset(results, 0, sizeof(*results));
    if (!G::spatialSearch || !G::callbackVtable) {
        Log("[autopet] [search] callback vtable 未定位");
        return false;
    }

    SearchCallback callback = {};
    // ChestSort 权威布局：
    //   storage+0x00 = vtable 地址
    //   storage+0x08 = &filterId（u64 指针）
    //   storage+0x10 = &results（结果向量指针）
    //   target = storage（指向自身）
    uint64_t localFilterId = filterId;
    *reinterpret_cast<void**>(callback.storage + 0x00) =
        reinterpret_cast<void*>(G::callbackVtable);
    *reinterpret_cast<uint64_t**>(callback.storage + 0x08) = &localFilterId;
    *reinterpret_cast<RawPointerVector**>(callback.storage + 0x10) = results;
    callback.target = callback.storage;

    // spatialSearch 的返回值不可信，依赖 results 向量填充
    __try {
        G::spatialSearch(spatialIndex, bounds, &callback, -1, -1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[autopet] SP spatialSearch 调用异常 (SEH)");
        return false;
    }

    // 搜索完成，必须销毁回调（vtable[4] destroy）
    if (callback.target) {
        __try {
            void** vtable = *reinterpret_cast<void***>(callback.target);
            using DestroyFunction = void(__fastcall*)(void*, bool);
            DestroyFunction destroy = reinterpret_cast<DestroyFunction>(vtable[4]);
            destroy(callback.target, callback.target != callback.storage);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[autopet] [search] destroy 调用异常 (SEH)");
        }
        callback.target = nullptr;
    }

    // 结果合理性校验
    // 空结果合法：搜索区间内无对象时 begin/end 均为 0（不是失败）
    if (results->begin == nullptr && results->end == nullptr) {
        return true;
    }
    if (!results->begin || !results->end || results->end < results->begin ||
        ((uintptr_t)results->end - (uintptr_t)results->begin) % sizeof(void*) != 0) {
        Log("[autopet] SP spatial result rejected begin=0x%llx end=0x%llx",
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(results->begin)),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(results->end)));
        return false;
    }
    return true;
}

// ============================================================
// 世界上下文（复用 ChestSort 链路）
// ============================================================
struct WorldContext {
    void* player;
    void* save;          // v0.4.6: save 对象（= root+0x208），用于 livestockList_ 遍历
    float position[4];
    void* spatialIndex;
};

static bool GetWorldContext(WorldContext* ctx) {
    if (!ctx) return false;
    memset(ctx, 0, sizeof(*ctx));

    // 第 1 步：读取 game root 全局指针
    void** rootSlot = (void**)(G::base + rva::GAME_ROOT);
    if (!IsReadable(rootSlot, sizeof(void*))) return false;
    void* root = *rootSlot;
    if (!root) return false;

    // 第 2 步：root+0x208 → player (save对象)
    void* player = nullptr;
    if (!ReadPtr(root, rva::ROOT_PLAYER_OFFSET, &player)) return false;

    // v0.4.6: player 即 save 对象（root+0x208），livestockList_ 在 save+0x3430
    ctx->save = player;

    // 第 3 步：player → objectStatus（= playerStatus）
    void* objectStatus = nullptr;
    if (!ReadPtr(player, rva::PLAYER_OBJECT_STATUS_OFFSET, &objectStatus)) return false;

    // 第 4 步：玩家位置（+0xf0）
    const float* pos = (const float*)((uintptr_t)objectStatus + rva::OBJECT_POSITION_OFFSET);
    if (!IsReadable(pos, sizeof(float) * 4)) return false;
    memcpy(ctx->position, pos, sizeof(float) * 4);
    if (!std::isfinite(ctx->position[0]) || !std::isfinite(ctx->position[1])) return false;
    if (fabsf(ctx->position[0]) < 1.0f && fabsf(ctx->position[1]) < 1.0f) return false;

    // 第 5 步：root+0x268 → mapOwner → mapInfo → spatialOwner → spatialIndex
    void* mapOwner = nullptr;
    void* mapInfo = nullptr;
    void* spatialOwner = nullptr;
    if (!ReadPtr(root, rva::ROOT_MAP_OWNER_OFFSET, &mapOwner)) return false;
    if (!ReadPtr(mapOwner, rva::MAP_INFO_OFFSET, &mapInfo)) return false;
    if (!ReadPtr(mapInfo, rva::MAP_SPATIAL_OWNER_OFFSET, &spatialOwner)) return false;

    void* spatialIndex = (void*)((uintptr_t)spatialOwner + rva::MAP_SPATIAL_INDEX_OFFSET);
    if (!IsReadable(spatialIndex, 0x48)) return false;

    ctx->player = player;
    ctx->spatialIndex = spatialIndex;
    return true;
}

// ============================================================
// caress key-value map 只读读取（移植 BigL233 AutoPetReadCaressState）
// ============================================================
static bool ReadCaressState(void* status, int* valueOut) {
    if (!status || !valueOut) return false;
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(status);
    const uintptr_t mapBase = reinterpret_cast<uintptr_t>(status) + field::CA_MAP_BASE;

    if (!IsReadable(reinterpret_cast<const void*>(mapBase + field::MAP_SENTINEL), 0x38))
        return false;

    void* sentinel = *reinterpret_cast<void* const*>(mapBase + field::MAP_SENTINEL);
    const uint64_t size = *reinterpret_cast<const uint64_t*>(mapBase + field::MAP_SIZE);
    void* buckets = *reinterpret_cast<void* const*>(mapBase + field::MAP_BUCKETS);
    const uint64_t mask = *reinterpret_cast<const uint64_t*>(mapBase + field::MAP_MASK);

    // 合理性校验（BigL233 同款）
    if (!sentinel || !IsReadable(sentinel, 0x20) || size > 1024 ||
        mask > 1023 || ((mask + 1) & mask) != 0 || !buckets ||
        !IsReadable(buckets, static_cast<size_t>(mask + 1) * 16)) {
        Log("[autopet] [map] 布局异常 sentinel=0x%llx size=%llu buckets=0x%llx mask=0x%llx",
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(sentinel)),
            static_cast<unsigned long long>(size),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(buckets)),
            static_cast<unsigned long long>(mask));
        return false;
    }

    const uint64_t bucket = CARESS_KEY & mask;
    void** pair = reinterpret_cast<void**>(
        reinterpret_cast<unsigned char*>(buckets) + bucket * 16);
    void* stop = pair[0];
    void* node = pair[1];
    if (size == 0 && node != sentinel) return false;
    if ((node == sentinel) != (stop == sentinel)) return false;
    if (node == sentinel) {
        *valueOut = 0;
        return true;
    }
    size_t visited = 0;
    bool reachedStop = false;
    while (node != sentinel && visited <= size) {
        if (!node || !IsReadable(node, 0x20)) return false;
        if (*reinterpret_cast<const uint64_t*>(
                reinterpret_cast<unsigned char*>(node) + 0x10) == CARESS_KEY) {
            const int value = *reinterpret_cast<const int*>(
                reinterpret_cast<unsigned char*>(node) + 0x18);
            if (value != 0 && value != 1) return false;
            *valueOut = value;
            return true;
        }
        ++visited;
        if (node == stop) { reachedStop = true; break; }
        node = *reinterpret_cast<void**>(
            reinterpret_cast<unsigned char*>(node) + sizeof(void*));
    }
    if (!reachedStop || visited > size) return false;
    *valueOut = 0; // 原生 getter 对缺失 key 返回 0
    return true;
}

// ============================================================
// 靠近检测 + 原生结算（v0.3.0 核心）
// ============================================================
static constexpr float SEARCH_RADIUS = 720.0f;   // 空间搜索半径
static constexpr float PET_RADIUS    = 220.0f;   // 抚摸判定半径

struct PetCandidate {
    void* status;        // CLivestockStatus 对象
    uint64_t uniqueId;   // 唯一 ID
    float dist;          // 距离
    int love;            // 当前好感
    int caress;          // caress 标记（0/1）
};

// v0.4.6: 收集附近可抚摸家畜——改用 livestockList_ 链表遍历（非空间搜索）
// 参考实现 BigL233 auto_pet.inl:2664 AutoPetSnapshotResidents
static int CollectNearbyLivestock(void* save, const float* playerPos,
                                  PetCandidate* out, int maxOut) {
    if (!save || !playerPos || !out || maxOut <= 0) return 0;

    // 每轮扫描开始前重置区域缓存
    FastRegionReset();

    // 读取 livestockList_ 链表（save + 0x3430）
    // 布局：[void* sentinel] [u64 declared_count]
    uintptr_t listAddr = (uintptr_t)save + rva::LIVESTOCK_LIST_OFFSET;
    if (!IsReadable((void*)listAddr, sizeof(void*) + sizeof(uint64_t))) return 0;

    void* sentinel = *reinterpret_cast<void**>(listAddr);
    uint64_t declared = *reinterpret_cast<const uint64_t*>(listAddr + sizeof(void*));
    if (!sentinel || declared == 0 || declared > rva::MAX_LIVESTOCK_LIST) return 0;
    if (!IsReadable(sentinel, sizeof(void*) * 2)) return 0;

    // 链表第一个节点
    void* node = *reinterpret_cast<void**>(sentinel);
    void* previous = sentinel;

    int n = 0;
    uint64_t vtLivestock = 0, vtOther = 0;
    int diagLogged = 0;
    size_t visited = 0;

    while (visited < (size_t)declared && n < maxOut) {
        if (!node || node == sentinel) break;
        if (!IsReadable(node, sizeof(void*) * 3)) break;
        // back-pointer 一致性校验
        if (*reinterpret_cast<void**>((uintptr_t)node + sizeof(void*)) != previous) break;

        // node+0x10 = CLivestockStatus* status
        void* status = *reinterpret_cast<void**>((uintptr_t)node + sizeof(void*) * 2);
        if (!status) { previous = node; node = *reinterpret_cast<void**>(node); ++visited; continue; }

        // vtable 过滤（放宽：接受三个 vtable 覆盖所有动物类型）
        uint64_t objVtable = 0;
        if (!DirectRead(status, &objVtable, sizeof(objVtable))) {
            previous = node; node = *reinterpret_cast<void**>(node); ++visited; continue;
        }
        if (objVtable != G::livestockVtable &&
            objVtable != G::creatureVtable &&
            objVtable != G::comUnitLivestockVtable) {
            vtOther++;
            if (diagLogged < 5) {
                Log("[autopet] [diag] 非家畜对象[%zu] status=0x%llx vtable=0x%llx",
                    visited, (unsigned long long)(uintptr_t)status,
                    (unsigned long long)objVtable);
                diagLogged++;
            }
            previous = node; node = *reinterpret_cast<void**>(node); ++visited; continue;
        }
        vtLivestock++;

        uintptr_t addr = (uintptr_t)status;

        // LIVE 有效
        uintptr_t liveValidPtr = 0;
        DirectRead(reinterpret_cast<void*>(addr + field::LIVE_VALID), &liveValidPtr, sizeof(liveValidPtr));
        uint8_t liveValid = 1;
        if (liveValidPtr && IsReadable(reinterpret_cast<void*>(liveValidPtr), 1)) {
            DirectRead(reinterpret_cast<void*>(liveValidPtr), &liveValid, sizeof(liveValid));
        }

        // FEATURE 位
        uint8_t featureLow = 0, featureHigh = 0;
        DirectRead(reinterpret_cast<void*>(addr + field::FEATURE_LOW), &featureLow, sizeof(featureLow));
        DirectRead(reinterpret_cast<void*>(addr + field::FEATURE_HIGH), &featureHigh, sizeof(featureHigh));

        // UID + caress
        uint64_t uniqueId = 0;
        DirectRead(reinterpret_cast<void*>(addr + field::UNIQUE_ID), &uniqueId, sizeof(uniqueId));
        int caress = -1;
        ReadCaressState(status, &caress);

        // LIVE 有效过滤
        if (liveValidPtr && IsReadable(reinterpret_cast<void*>(liveValidPtr), 1)) {
            if (!liveValid) { previous = node; node = *reinterpret_cast<void**>(node); ++visited; continue; }
        }

        // FEATURE 位过滤——参考实现只检查最低 2 位 (& 0x03)：
        // (featureLow & 0x03) != 0 或 (featureHigh & 0x03) != 0 才是 HORROR/PIRO 事件动物
        // 狗的 featureLow=0x70，0x70 & 0x03 = 0，不是事件动物，应通过
        if ((featureLow & 0x03) != 0 || (featureHigh & 0x03) != 0) {
            previous = node; node = *reinterpret_cast<void**>(node); ++visited; continue;
        }

        // UID + LOVE
        int love = 0;
        DirectRead(reinterpret_cast<void*>(addr + field::LOVE), &love, sizeof(love));

        out[n++] = {status, uniqueId, 0.0f, love, caress};

        previous = node;
        node = *reinterpret_cast<void**>(node);
        ++visited;
    }

    // 诊断统计（降频）
    static uint64_t diagSeq = 0;
    if (++diagSeq % 5 == 0) {
        Log("[autopet] [diag] livestockList: 遍历=%zu 家畜=%llu 其他=%llu 已收集=%d",
            visited, (unsigned long long)vtLivestock,
            (unsigned long long)vtOther, n);
    }

    return n;
}

// 原生结算：love setter + caress 标记（全程不碰 foodNum_）
static bool SettlePet(PetCandidate* c) {
    if (!c || !c->status) return false;
    if (!G::loveSetter || !G::statusSetter || !G::rateModifier) {
        Log("[Pet] 原生结算函数未就绪 (love=%p status=%p rate=%p)",
            (void*)G::loveSetter, (void*)G::statusSetter, (void*)G::rateModifier);
        return false;
    }

    // 玩家 status（计算好感倍率用）
    void* playerStatus = nullptr;
    {
        void** rootSlot = (void**)(G::base + rva::GAME_ROOT);
        if (!IsReadable(rootSlot, sizeof(void*))) return false;
        void* root = *rootSlot;
        if (!root) return false;
        void* player = nullptr;
        if (!ReadPtr(root, rva::ROOT_PLAYER_OFFSET, &player)) return false;
        if (!ReadPtr(player, rva::PLAYER_OBJECT_STATUS_OFFSET, &playerStatus)) return false;
    }
    if (!playerStatus) return false;

    // 倍率（BigL233 同款：RateModifier(playerStatus, 11)）
    int64_t modifierResult = 0;
    __try {
        modifierResult = G::rateModifier(playerStatus, 11);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[Pet] rateModifier 调用异常 (SEH)");
        return false;
    }
    int32_t modifier = static_cast<int32_t>(modifierResult);
    int64_t scaledDelta = (static_cast<int64_t>(modifier) + 100) * 5;
    if (scaledDelta < -2147483647LL - 1 || scaledDelta > 2147483647LL) return false;
    int effectiveDelta = static_cast<int>(scaledDelta) / 100;

    int loveBefore = c->love;
    int64_t requestedLove = static_cast<int64_t>(loveBefore) + effectiveDelta;
    if (requestedLove < -2147483647LL - 1 || requestedLove > 2147483647LL) return false;

    // 写好感（原生 love setter）
    __try {
        G::loveSetter(c->status, static_cast<int>(requestedLove), false);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[Pet] loveSetter 调用异常 (SEH)");
        return false;
    }

    // 写 caress 每日标记（原生 status setter）
    const uint64_t key = CARESS_KEY;
    __try {
        G::statusSetter(reinterpret_cast<unsigned char*>(c->status) + 0x10, &key, 1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("[Pet] statusSetter 调用异常 (SEH)");
        return false;
    }

    // 回读验证
    int after = -1;
    bool afterOk = ReadCaressState(c->status, &after);
    Log("[Pet] settled uid=0x%llX dist=%.1f love %d -> %d (delta=%d) mod=%d caress=%d(ok=%d)",
        static_cast<unsigned long long>(c->uniqueId),
        c->dist, loveBefore, static_cast<int>(requestedLove), effectiveDelta,
        modifier, after, afterOk ? 1 : 0);
    return true;
}

// ============================================================
// HUD：左中显示已抚摸/总数（v0.5.0）
// ============================================================
static HMODULE g_autopetModule = nullptr;
static constexpr wchar_t PET_HUD_CLASS[] = L"AutoPetHudWindow";
static HWND g_petHudWindow = nullptr;
static HFONT g_petHudFont = nullptr;
static int g_petHudTotal = 0;
static int g_petHudPetted = 0;
static ULONGLONG g_petHudHideAt = 0;  // v0.5.2: 自动隐藏计时

static LRESULT CALLBACK PetHudWndProc(HWND window, UINT message,
                                      WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        HDC dc = BeginPaint(window, &paint);
        RECT client = {};
        GetClientRect(window, &client);

        HBRUSH bg = CreateSolidBrush(RGB(28, 30, 34));
        FillRect(dc, &client, bg);
        DeleteObject(bg);

        // 左侧强调色条（绿色=已全摸，灰色=未完成）
        RECT bar = client;
        bar.right = bar.left + 6;
        COLORREF accent = (g_petHudTotal > 0 && g_petHudPetted == g_petHudTotal)
            ? RGB(127, 176, 105) : RGB(200, 165, 70);
        HBRUSH accentBrush = CreateSolidBrush(accent);
        FillRect(dc, &bar, accentBrush);
        DeleteObject(accentBrush);

        SetBkMode(dc, TRANSPARENT);
        HFONT oldFont = (HFONT)SelectObject(dc, g_petHudFont);
        SetTextColor(dc, RGB(220, 225, 230));

        // "已抚摸 N/M 只动物" (v0.5.2: 修正 抚=U+629A)
        wchar_t buf[64];
        int len = swprintf_s(buf, 64, L"\x5DF2\x629A\x6478 %d/%d \x53EA\x52A8\x7269",
                             g_petHudPetted, g_petHudTotal);
        RECT r = client;
        r.left += 22; r.right -= 14; r.top += 6;
        r.bottom = r.top + 28;
        DrawTextW(dc, buf, len, &r,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(dc, oldFont);
        EndPaint(window, &paint);
        return 0;
    }
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

static bool PetHudInit() {
    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = PetHudWndProc;
    cls.hInstance = g_autopetModule;
    cls.lpszClassName = PET_HUD_CLASS;
    if (!RegisterClassExW(&cls)) {
        DWORD err = GetLastError();
        WNDCLASSEXW existing = {};
        existing.cbSize = sizeof(existing);
        if (err != ERROR_CLASS_ALREADY_EXISTS ||
            !GetClassInfoExW(g_autopetModule, PET_HUD_CLASS, &existing) ||
            existing.lpfnWndProc != PetHudWndProc) return false;
    }

    g_petHudFont = CreateFontW(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    if (!g_petHudFont) return false;

    const int width = 200;
    const int height = 40;
    g_petHudWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        PET_HUD_CLASS, L"", WS_POPUP, 0, 0, width, height,
        nullptr, nullptr, g_autopetModule, nullptr);
    if (!g_petHudWindow) return false;

    SetLayeredWindowAttributes(g_petHudWindow, 0, 228, LWA_ALPHA);
    HRGN rounded = CreateRoundRectRgn(0, 0, width + 1, height + 1, 10, 10);
    if (!SetWindowRgn(g_petHudWindow, rounded, FALSE)) DeleteObject(rounded);
    return true;
}

static void PetHudUpdate(DWORD durationMs = 5000) {
    if (!g_petHudWindow && !PetHudInit()) return;
    if (!g_petHudWindow) return;

    // 左中位置
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
    if (!GetMonitorInfoW(mon, &mi)) {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }
    const int width = 200;
    const int height = 40;
    const int x = mi.rcWork.left + 24;
    const int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - height) / 2;
    SetWindowPos(g_petHudWindow, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(g_petHudWindow, nullptr, TRUE);
    UpdateWindow(g_petHudWindow);
    g_petHudHideAt = GetTickCount64() + durationMs;  // v0.5.2: 设定隐藏时间
}

static void PetHudPump() {
    if (!g_petHudWindow) return;
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    // v0.5.2: 超时自动隐藏
    if (IsWindowVisible(g_petHudWindow) && g_petHudHideAt != 0 &&
        GetTickCount64() >= g_petHudHideAt) {
        ShowWindow(g_petHudWindow, SW_HIDE);
        g_petHudHideAt = 0;
    }
}

// ============================================================
// 每帧逻辑
// ============================================================
static void AutoPetTickImpl() {
    if (!G::vtableVerified || !G::livestockVtable) return;

    WorldContext ctx = {};
    if (!GetWorldContext(&ctx)) {
        return; // 游戏加载中属正常，不刷日志
    }

    // v0.4.6: 改用 livestockList_ 链表遍历（非空间搜索），覆盖牛/马/狗等所有动物
    PetCandidate candidates[32];
    int n = CollectNearbyLivestock(ctx.save, ctx.position, candidates, 32);
    if (n <= 0) {
        g_petHudTotal = 0;
        g_petHudPetted = 0;
        PetHudUpdate();
        return;
    }

    for (int i = 0; i < n; i++) {
        // 已抚摸过（caress=1）跳过
        if (candidates[i].caress == 1) continue;
        SettlePet(&candidates[i]);
    }

    // v0.5.2: HUD 统计——结算后重新读 caress 状态，统计实际已摸数量
    g_petHudTotal = n;
    g_petHudPetted = 0;
    for (int i = 0; i < n; i++) {
        int caress = -1;
        ReadCaressState(candidates[i].status, &caress);
        if (caress == 1) g_petHudPetted++;
    }
    PetHudUpdate();
}

// ============================================================
// Feature 实现
// ============================================================
class AutoPetFeature : public qol::Feature {
public:
    AutoPetFeature() : Feature("autopet") {}

    bool Init() override {
        G::base = reinterpret_cast<uintptr_t>(GetModuleHandleA("village.exe"));
        if (!G::base) {
            Log("[autopet] FATAL: village.exe module not found");
            return false;
        }
        Log("[autopet] mod_init() base=0x%llx %s",
            static_cast<unsigned long long>(G::base), AUTOPET_VERSION);

        // 计算 vtable 绝对地址
        G::livestockVtable = G::base + rva::LIVESTOCK_VTABLE;
        G::creatureVtable = G::base + rva::CREATURE_VTABLE;
        G::comUnitLivestockVtable = G::base + rva::COM_UNIT_LIVESTOCK_VTABLE;

        // 验证 vtable（读 vtable[-1] 确认 COL sig=1）
        bool ok1 = VerifyVtable(rva::LIVESTOCK_VTABLE, "CLivestockStatus");
        bool ok2 = VerifyVtable(rva::CREATURE_VTABLE, "CCreatureStatus");
        bool ok3 = VerifyVtable(rva::COM_UNIT_LIVESTOCK_VTABLE, "CCom_Unit_Livestock");

        G::vtableVerified = ok1; // 只需主 vtable 验证通过即可

        if (!G::vtableVerified) {
            Log("[autopet] vtable verification FAILED — feature disabled");
            return false;
        }

        // AOB 扫描空间搜索函数
        for (size_t i = 0; i < kAOBCount; i++) {
            uintptr_t addr = qol::ScanModuleAOB(nullptr, kAOBs[i].hex);
            if (!addr) {
                Log("[autopet] AOB scan FAILED: %s", kAOBs[i].name);
                return false;
            }
            Log("[autopet] AOB %s -> 0x%p", kAOBs[i].name, (void*)addr);
            if (i == IDX_SPATIAL_SEARCH) {
                G::spatialSearch = (G::FnSpatialSearch)addr;
            } else if (i == IDX_RAW_VECTOR_FREE) {
                G::rawVectorFree = (G::FnRawVectorFree)addr;
            }
        }

        // 回调 vtable 硬编码（RTTI 链已验证）
        G::callbackVtable = G::base + rva::SEARCH_CALLBACK_VTABLE;
        Log("[autopet] callback vtable=0x%llx", static_cast<unsigned long long>(G::callbackVtable));

        // 原生结算函数（v0.3.0）
        G::rateModifier = reinterpret_cast<G::FnRateModifier>(G::base + rva::RATE_MODIFIER);
        G::loveSetter = reinterpret_cast<G::FnLoveSetter>(G::base + rva::LOVE_SETTER);
        G::statusSetter = reinterpret_cast<G::FnStatusSetter>(G::base + rva::STATUS_SETTER);
        Log("[autopet] native funcs: rate=0x%llx love=0x%llx status=0x%llx",
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(G::rateModifier)),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(G::loveSetter)),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(G::statusSetter)));

        Log("[autopet] init OK: livestockVt=0x%llx creatureVt=0x%llx comUnitVt=0x%llx",
            static_cast<unsigned long long>(G::livestockVtable),
            static_cast<unsigned long long>(G::creatureVtable),
            static_cast<unsigned long long>(G::comUnitLivestockVtable));
        return true;
    }

    void Tick() override {
        if (!G::vtableVerified) return;

        PetHudPump();

        // 节流：每 SCAN_INTERVAL_TICKS 检查一次天数变化
        uint64_t now = ++G::lastScanTick;
        if (now % G::SCAN_INTERVAL_TICKS != 0) return;

        // v0.5.1: 读游戏时间判断是否跨日
        WorldContext ctx = {};
        if (!GetWorldContext(&ctx)) return;

        // 读 raw_second
        int64_t rawSecond = 0;
        if (!IsReadable(reinterpret_cast<void*>((uintptr_t)ctx.save + rva::RAW_SECOND_OFFSET), sizeof(int64_t))) return;
        DirectRead(reinterpret_cast<void*>((uintptr_t)ctx.save + rva::RAW_SECOND_OFFSET), &rawSecond, sizeof(rawSecond));
        if (rawSecond < 0) return;

        int64_t dayId = rawSecond / rva::SECONDS_PER_DAY;

        // 首次扫描：延迟 ~10秒（等场景加载）后执行一次
        if (G::lastDayId < 0) {
            if (G::firstScanDelayTicks < 600) {  // ~10秒@60fps
                G::firstScanDelayTicks += G::SCAN_INTERVAL_TICKS;
                return;
            }
            G::lastDayId = dayId;
            Log("[autopet] 首次扫描 dayId=%lld", (long long)dayId);
            AutoPetTickImpl();
            return;
        }

        // 同一天：不扫描
        if (dayId == G::lastDayId) return;

        // 跨日了！执行抚摸（caress 已被原生重置为 0）
        G::lastDayId = dayId;
        Log("[autopet] 检测到跨日 dayId %lld -> %lld，执行抚摸",
            (long long)(dayId - 1), (long long)dayId);
        AutoPetTickImpl();
    }
};

AutoPetFeature g_feature;

// ============================================================
// 插件入口（加载器要求：mod_init / mod_tick / unload）
// ============================================================

extern "C" __declspec(dllexport) void mod_init(void) {
    if (!SelfVerifyInit("autopet")) return;
    Log("[autopet] mod_init() called");
}

extern "C" __declspec(dllexport) void mod_tick(void) {
    g_feature.SafeTick();
}

extern "C" __declspec(dllexport) void unload(void) {
    Log("[autopet] unload() called");
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID /*lpReserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_autopetModule = h;  // v0.5.0: HUD 需要模块句柄
        qol::SetLogName("autopet");
        Log("[autopet] DllMain attach \u2014 %s build 25094764", AUTOPET_VERSION);
        g_feature.SafeInit();
    } else if (reason == DLL_PROCESS_DETACH) {
        Log("[autopet] detach");
    }
    return TRUE;
}