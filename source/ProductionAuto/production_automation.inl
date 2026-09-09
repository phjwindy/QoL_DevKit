// ---- production equipment automation (v1.8 diagnostic gate) ------------
//
// The current executable proves that CSaveData::gimmickList_ is the canonical
// serialized CGimmickStatus registry.  Static analysis does not by itself prove
// that every unloaded-map entry remains resident at every lifecycle point, so
// this module reports registry observations without claiming live off-map
// execution.  It never retains a game pointer after the callback: the worker/UI
// side receives fixed-size value snapshots only.
//
// Native GimmickProcessPushItem is player-inventory coupled and remains
// disabled.  Output delivery uses the completion helper, a rank-aware chest
// commit, and the same integer status-map/event chain used by the native Lua
// bridge to finish inProc after the last slot.  It runs before the game's
// native update, one slot per callback, with binding/live-object revalidation
// immediately before and after the helper.
// Binding, stable-ID resolution and on-demand all-map observation are active;
// unsafe transfer requests stop in FAULTED with an explicit reason.  Processing
// machines have no native menu: the mod panel is requested only by F3 or normal-
// world D-pad Right while the engine's own selected interaction record resolves
// to one exact recipe-backed production machine ID/module pair.  The selected
// selfObjID can be a collision/sensor child rather than the Gimmick world
// object.  The current executable exposes the same logical hierarchy used by
// Lua ObjectGetParent/ObjectAddChild: child+0x08 is the parent and
// parent+0x350..0x360 is the intrusive child vector.  The selected sensor is
// resolved through that bounded, mutually
// verified chain to its exact Gimmick root; adjacent machines are never chosen
// by proximity.

#pragma once

#include "processing_recipe_table.inl"

// RVA_PRODUCTION_* 常量已在 version_manifest.h 中定义为宏，此处不再重复定义
// CGimmickStatus is multiply inherited.  The secondary vptr lives at
// status+0x10; its current-build COL has offset=0x10 and shares the exact
// CGimmickStatus type descriptor/hierarchy with the primary vtable.  Keeping
// this as a named, install-verified RVA prevents a stale predecessor vtable
// address from silently rejecting every live registry status.

static constexpr uintptr_t PRODUCTION_SAVE_OFFSET = 0x208;
static constexpr uintptr_t PRODUCTION_PLAYER_STATUS_OFFSET = 0x32b8;
static constexpr uintptr_t PRODUCTION_PLAYER_CONTROLLER_OFFSET = 0x410;
static constexpr uintptr_t PRODUCTION_SELECTED_OBJECT_ID_OFFSET = 0x4a0;
static constexpr uintptr_t PRODUCTION_SELECTED_TARGET_POSITION_OFFSET = 0x4b0;
static constexpr uintptr_t PRODUCTION_SELECTED_TARGET_ANGLE_OFFSET = 0x4c0;
static constexpr uintptr_t PRODUCTION_SELECTED_COMMAND_HOLDER_OFFSET = 0x4c8;
static constexpr uintptr_t PRODUCTION_COMPONENT_STATUS_OFFSET = 0x338;
static constexpr uintptr_t PRODUCTION_WORLD_PARENT_OFFSET = 0x008;
static constexpr uintptr_t PRODUCTION_WORLD_CHILDREN_BEGIN_OFFSET = 0x350;
static constexpr uintptr_t PRODUCTION_WORLD_CHILDREN_END_OFFSET = 0x358;
static constexpr uintptr_t PRODUCTION_WORLD_CHILDREN_CAPACITY_OFFSET = 0x360;
static constexpr uintptr_t PRODUCTION_STATUS_OBJECT_LINK_OFFSET = 0x208;
static constexpr uintptr_t PRODUCTION_LINK_BODY_OFFSET = 0x008;
static constexpr uintptr_t PRODUCTION_WORLD_OBJECT_ID_OFFSET = 0x028;
static constexpr uintptr_t PRODUCTION_GIMMICK_LIST_OFFSET = 0x3440;
static constexpr uintptr_t PRODUCTION_UNIQUE_ID_OFFSET = 0x0e0;
static constexpr uintptr_t PRODUCTION_MAP_HOLDER_OFFSET = 0x0e8;
static constexpr uintptr_t PRODUCTION_POSITION_OFFSET = 0x0f0;
static constexpr uintptr_t PRODUCTION_GIMMICK_DATA_OFFSET = 0x240;
static constexpr uintptr_t PRODUCTION_TIME_VALUE_OFFSET = 0x288;
static constexpr uintptr_t PRODUCTION_CREATE_TIME_OFFSET = 0x2a0;
static constexpr uintptr_t PRODUCTION_ITEM_LIST_OFFSET = 0x2b8;
static constexpr uintptr_t PRODUCTION_NAME_OFFSET = 0x2e0;
static constexpr uintptr_t PRODUCTION_UNIQUE_NAME_POINTER_OFFSET = 0x2e8;
static constexpr size_t PRODUCTION_MACHINE_SLOTS = 3;
static constexpr size_t PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT = 8;
static constexpr size_t PRODUCTION_CHEST_SLOTS = 64;
static constexpr size_t PRODUCTION_MAX_GIMMICKS = 16384;
static constexpr size_t PRODUCTION_MAX_CHESTS = 512;
static constexpr size_t PRODUCTION_NEAREST_CHESTS_PER_BINDING = 4;
static constexpr size_t PRODUCTION_MAX_BINDINGS = 256;
static constexpr size_t PRODUCTION_MAX_FLOORS = 1024;
static constexpr size_t PRODUCTION_MAX_OBJECT_SNAPSHOTS =
    PRODUCTION_MAX_CHESTS + PRODUCTION_MAX_BINDINGS + PRODUCTION_MAX_FLOORS;
static constexpr size_t PRODUCTION_MAX_NAME_BYTES = 95;
static constexpr size_t PRODUCTION_MAX_MODULE_BYTES = 47;
static constexpr size_t PRODUCTION_SCAN_BATCH_OBJECTS = 128;
static constexpr LONGLONG PRODUCTION_SCAN_BUDGET_US = 2000;
static constexpr size_t PRODUCTION_MAX_PARENT_DEPTH = 8;
static constexpr size_t PRODUCTION_MAX_PARENT_CHILDREN = 4096;
static constexpr u64 PRODUCTION_DPAD_RIGHT_COMMAND = 0x3fa;
static constexpr u64 PRODUCTION_ITEM_IN_COMMAND = 0x435;
static constexpr u64 PRODUCTION_ITEM_OUT_COMMAND = 0x436;
static constexpr bool PRODUCTION_NATIVE_TRANSACTIONS_PROVEN = true;
static constexpr char PRODUCTION_RECIPE_CATALOG_SHA256[] =
    "09C5BD2BC8C732E2B66159DE5CE045E024C9AC718FE8D306B26594F991732D35";

// New opaque v1.8 release seal.  This is public build identity, never a key.
#if defined(__GNUC__)
__attribute__((used, section(".rdata$pafs")))
#endif
static const volatile unsigned char g_productionAutomationBuildSeal[16] = {
    0x29, 0x58, 0x48, 0xbb, 0x4f, 0xce, 0x71, 0xa5,
    0xdb, 0x21, 0x2a, 0x9e, 0xf4, 0xaf, 0x3c, 0xec,
};
static constexpr u32 PRODUCTION_AUTOMATION_BUILD_SEAL_FNV1A = 0x4de3b5d3u;

enum class ProductionState : int {
    Unbound = 0,
    Idle,
    WaitingInput,
    Starting,
    Running,
    OutputReady,
    WaitingOutputSpace,
    Faulted,
};

enum class ProductionReason : int {
    None = 0,
    InputUnbound,
    OutputUnbound,
    PausedByUser,
    WaitingIngredients,
    RunningNativeTimer,
    OutputNeedsTransfer,
    OutputCapacityInsufficient,
    ChestNameInvalid,
    ChestIdentityMissing,
    MachineIdentityMissing,
    AmbiguousIdentity,
    InvalidObjectType,
    SameChestRejected,
    NativeTransactionEvidenceRequired,
    RegistryInvalid,
    MainThreadChanged,
    ConfigCorrupt,
    BindingSettlePending,
};

enum class ProductionModalState : unsigned {
    Hidden = 0,
    Opening,
    Active,
    ClosingDrain,
};

// Runtime-only scheduler state.  These values are deliberately stored beside
// the stable binding identity: no registry node, status, inventory, or world
// object pointer survives the callback which resolved it.
enum class ProductionDueKind : unsigned char {
    Dirty = 0,
    Finish,
    OutputRetry,
    InputRetry,
    Parked,
};

static const char* ProductionDueKindName(ProductionDueKind kind) {
    switch (kind) {
        case ProductionDueKind::Dirty: return "Dirty";
        case ProductionDueKind::Finish: return "Finish";
        case ProductionDueKind::OutputRetry: return "OutputRetry";
        case ProductionDueKind::InputRetry: return "InputRetry";
        case ProductionDueKind::Parked: return "Parked";
    }
    return "Unknown";
}

static constexpr unsigned PRODUCTION_MODAL_KEYBOARD_F3 = 0x01;
static constexpr unsigned PRODUCTION_MODAL_CONTROLLER_DPAD_RIGHT = 0x02;

struct ProductionStableId {
    u64 mapId;
    u64 uniqueId;
};

static bool ProductionIdEqual(const ProductionStableId& left,
                              const ProductionStableId& right) {
    return left.mapId == right.mapId && left.uniqueId == right.uniqueId;
}


struct ProductionBinding {
    ProductionStableId machine;
    ProductionStableId input;
    ProductionStableId output;
    bool hasInput;
    bool hasOutput;
    bool enabled;
    bool committed;
    bool autoLinked;  // true = created by chain auto-linker (relaxes same-chest)
    ProductionState state;
    ProductionReason reason;
    std::int64_t nextDueGameSecond;
    std::int64_t expectedFinishGameSecond;
    u32 retryGameTicks;
    ProductionDueKind dueKind;
    // Flood-fill group: index into g_productionChestGroups, or SIZE_MAX.
    // When the current input/output chest fails (no recipe / full), the
    // state machine rotates to the next chest in the group.
    size_t groupIndex;
    uint8_t inputRotation;   // which chest in the group we're trying for input
    uint8_t outputRotation;  // which chest in the group we're trying for output
    uint8_t inputTriedCount;   // distinct input chests tried since last reset
    uint8_t outputTriedCount;  // distinct output chests tried since last reset
    // Nearest chests to this machine, sorted by distance.  Only these
    // chests are eligible for input/output rotation, preventing a machine
    // from pulling from / pushing to chests that are far away in a large
    // flood-fill group.
    ProductionStableId nearestChestIds[PRODUCTION_NEAREST_CHESTS_PER_BINDING];
    uint8_t nearestChestCount;
    // Input chest content fingerprint for change-triggered retry.  When a
    // WaitingInput binding fails to find a satisfiable recipe, this records
    // the hash of the input chest inventory.  A later scan that observes a
    // different hash (player added ingredients) wakes the binding to Dirty
    // instead of waiting out the exponential input backoff.
    uint64_t inputFingerprint;
};



struct ProductionObjectSnapshot {
    ProductionStableId id;
    u64 gimmickId;
    char module[PRODUCTION_MAX_MODULE_BYTES + 1];
    char name[PRODUCTION_MAX_NAME_BYTES + 1];
    float position[3];
    bool named;
    bool chest;
    bool machine;
    bool floor;
    bool inventoryValid;
    bool processValid;
    size_t slotLimit;
    u64 start[PRODUCTION_MACHINE_SLOTS];
    u64 duration[PRODUCTION_MACHINE_SLOTS];
    u64 outputItemId[PRODUCTION_MACHINE_SLOTS];
    int outputCount[PRODUCTION_MACHINE_SLOTS];
    u64 inputItemId[PRODUCTION_MACHINE_SLOTS];
    u32 inputCount[PRODUCTION_MACHINE_SLOTS];
    size_t occupiedSlots;
};

struct ProductionPublishedState {
    u64 generation;
    std::int64_t gameSecond;
    ProductionStableId activeMachine;
    bool activeMachinePresent;
    ProductionState activeState;
    ProductionReason activeReason;
    bool scanActive;
    size_t scanProcessed;
    size_t scanTotal;
    size_t chestCount;
    ProductionObjectSnapshot chests[PRODUCTION_MAX_CHESTS];
};

static SRWLOCK g_productionBindingLock = SRWLOCK_INIT;
static SRWLOCK g_productionPublishLock = SRWLOCK_INIT;
static SRWLOCK g_productionActiveLock = SRWLOCK_INIT;
static SRWLOCK g_productionLogLock = SRWLOCK_INIT;
static SRWLOCK g_productionModalAcquireLock = SRWLOCK_INIT;
static ProductionBinding g_productionBindings[PRODUCTION_MAX_BINDINGS] = {};
static size_t g_productionBindingCount = 0;

// ---- Flood-fill chest groups ------------------------------------------------
// After each scan, machines and chests that are adjacent (within
// PRODUCTION_AUTOLINK_RADIUS) are flood-filled into connected groups.
// A machine in a group can take input from ANY chest in the group and
// deliver output to ANY chest in the group.  When the current chest
// fails (no recipe / full), the binding rotates to the next chest.
static constexpr size_t PRODUCTION_MAX_GROUPS = 128;
static constexpr size_t PRODUCTION_MAX_CHESTS_PER_GROUP = 16;
static constexpr size_t PRODUCTION_MAX_MACHINES_PER_GROUP = 16;

struct ProductionChestGroup {
    ProductionStableId chests[PRODUCTION_MAX_CHESTS_PER_GROUP];
    size_t chestCount;
    ProductionStableId machines[PRODUCTION_MAX_MACHINES_PER_GROUP];
    size_t machineCount;
};
static ProductionChestGroup g_productionChestGroups[PRODUCTION_MAX_GROUPS] = {};
static size_t g_productionChestGroupCount = 0;

// ---- Chest settle detection -----------------------------------------------
// A newly placed chest must exist for PRODUCTION_CHEST_SETTLE_MS before it
// is included in the flood-fill.  This prevents binding churn while the
// player is actively placing/moving chests.  Existing chests that were
// already seen in a previous scan are always settled.
struct ProductionChestSettleRecord {
    ProductionStableId id;
    ULONGLONG firstSeenTick;
    bool valid;
    bool seenThisScan;
};
static ProductionChestSettleRecord
    g_productionChestSettle[PRODUCTION_MAX_CHESTS] = {};
// Pre-computed settle flags, indexed by object position in objects[].
static bool g_productionChestSettled[PRODUCTION_MAX_OBJECT_SNAPSHOTS] = {};
// True until the first AutoLink scan completes.  On the first scan all
// existing chests are treated as settled (they were already in position
// when the save loaded).
static bool g_productionChestSettleFirstScan = true;

static ProductionPublishedState g_productionPublished = {};
static std::atomic<bool> g_productionReady{false};
static std::atomic<bool> g_productionFaulted{false};
static std::atomic<DWORD> g_productionMainThreadId{0};
static std::atomic<u64> g_productionLogSequence{0};
static std::atomic<u64> g_productionSequence{0};
static std::atomic<u64> g_productionTxnSequence{0};
static ProductionStableId g_productionActiveMachine = {};
// g_productionWorldScheduleEpoch 移至 production_auto.cpp（cpp 在 .inl include 前引用）
static std::atomic<bool> g_productionRegistryScanRequested{false};
static std::atomic<bool> g_productionScanInProgress{false};
static std::atomic<DWORD> g_productionScanOwnerThread{0};
static std::atomic<ULONGLONG> g_productionLastWorldCallbackTick{0};
static std::atomic<bool> g_productionWorldContextActive{false};
static std::atomic<ULONGLONG> g_productionMainCallbackEnteredTick{0};
static std::atomic<ULONGLONG> g_productionMainCallbackCompletedTick{0};
static std::atomic<ULONGLONG> g_productionMainCallbackDurationMs{0};
static std::atomic<u64> g_productionMainCallbackEnteredSequence{0};
static std::atomic<u64> g_productionMainCallbackCompletedSequence{0};
static std::atomic<unsigned> g_productionMainCallbackPhase{0};
static std::atomic<bool> g_productionMainCallbackInFlight{false};
static std::atomic<bool> g_productionWorkerTimeoutObserved{false};
static std::atomic<bool> g_productionTransferInFlight{false};
static std::atomic<u64> g_productionBindingRevision{1};
static std::atomic<ULONGLONG> g_productionTransferNotBeforeTick{0};
static std::atomic<bool> g_productionBindingMutationInProgress{false};
static std::atomic<u64> g_productionTransferMutationSequence{0};
static std::atomic<bool> g_productionMutationDeferredLogged{false};
static std::atomic<unsigned> g_productionPreUpdateStage{0};
// One CAS token owns generation, lifecycle state and opener/closer edge mask.
// Keeping them in one atomic prevents a stale worker release from clearing a
// newer main-thread panel request.
static std::atomic<u64> g_productionModalToken{0};
static std::atomic<HWND> g_productionModalOpeningOwner{nullptr};
static std::atomic<u64> g_productionModalOpeningOwnerGeneration{0};
static std::atomic<u64> g_productionModalDrainRequiredCallback{0};
static std::atomic<u64> g_productionModalDrainCompletedGeneration{0};
static std::atomic<u64> g_productionModalDrainPhysicalGeneration{0};
static thread_local unsigned g_productionMainCallbackDepth = 0;
static thread_local ULONGLONG g_productionMainCallbackStartedAt = 0;
static thread_local bool g_productionScanning = false;
static thread_local bool g_productionScanActive = false;
static thread_local size_t g_productionScanCursor = 0;
static thread_local size_t g_productionScanExpected = 0;
static thread_local size_t g_productionScanRegistryIndexCount = 0;
static thread_local size_t g_productionScanObjectCount = 0;
static thread_local size_t g_productionScanMachineCount = 0;
static thread_local size_t g_productionScanNamedChestCount = 0;
static thread_local bool g_productionScanObjectTruncated = false;
static thread_local LONGLONG g_productionScanStartedAt = 0;
static thread_local u64 g_productionScanPrefixHash =
    1469598103934665603ULL;
static thread_local void* g_productionScanResumeNode = nullptr;
static thread_local void* g_productionScanResumePrev = nullptr;
static ProductionObjectSnapshot
    g_productionScanObjects[PRODUCTION_MAX_OBJECT_SNAPSHOTS] = {};

// Scan generation: incremented every time a scan completes and publishes
// Scan generation: incremented every time a scan completes and publishes
// new object snapshots.  Used to skip redundant ProductionFindObject
// calls when the scan data has not changed since the last refresh.
static u64 g_productionScanGeneration = 0;

// Forward declarations: the registry index cache is defined later in this
// file (~L5461) but is used by ProductionFindObject (~L1503) which runs
// earlier.  These declarations allow the fast-path lookup to compile.
static size_t g_productionRegistryIndexCacheDeclared = 0;
static bool ProductionRegistryIndexCacheLookup(
        const ProductionStableId& id, size_t declared, u64 loadGeneration,
        size_t* indexOut, bool* uniqueOut);

static u64 ProductionNextLogSequence() {
    return g_productionLogSequence.fetch_add(1, std::memory_order_relaxed) + 1;
}

static ProductionStableId ProductionGetActiveMachine() {
    ProductionStableId active = {};
    AcquireSRWLockShared(&g_productionActiveLock);
    active = g_productionActiveMachine;
    ReleaseSRWLockShared(&g_productionActiveLock);
    return active;
}

static void ProductionSetActiveMachineId(const ProductionStableId& active) {
    AcquireSRWLockExclusive(&g_productionActiveLock);
    g_productionActiveMachine = active;
    ReleaseSRWLockExclusive(&g_productionActiveLock);
}

// Value-only scheduler prime.  The definition lives beside the due scheduler,
// but config/world/load lifecycle code may request it before the 5-second
// settle gate opens.  It never resolves a registry node or a game pointer.
static void ProductionPrimeAllEnabledBindingsDirty(const char* reason);

// Due work must never touch registry or chest data in the first
// moments after a world-context (re)activation: a save load restores machines
// with finished products while the game is still settling its world/script/
// save systems, and touching that data right after a load froze the game.
// The refresh therefore waits for the load lifecycle to provably settle.
static ULONGLONG g_productionWorldActiveSince = 0;
static bool g_productionTransferDeferredLogged = false;
// Main-thread-only lifecycle values.  AutoPet publishes a morning generation
// only after the verified natural reload cleanup returned.  When the following
// callback observes the replacement input identity, that generation permits
// the natural morning edge (and only that edge) to skip the generic world-load
// wall-clock settle while the normal load-generation gates remain mandatory.
static u64 g_productionNaturalMorningGenerationObserved = 0;
static u64 g_productionNaturalMorningEdgePending = 0;
static bool g_productionNaturalMorningSettleReady = false;
static constexpr ULONGLONG PRODUCTION_TRANSFER_SETTLE_MS = 5000;
static constexpr ULONGLONG PRODUCTION_POST_SCAN_SETTLE_MS = 5000;
static constexpr ULONGLONG PRODUCTION_MAX_SAFE_CALLBACK_MS = 250;
// Escape window for sessions that never observe a save-load at all (a
// brand-new game, or AutoPet's load lifecycle unavailable): only a world
// context that has stayed continuously valid this long can prove that no
// load finalization is still pending.
static constexpr ULONGLONG PRODUCTION_NEW_GAME_SETTLE_MS = 45000;
static constexpr size_t PRODUCTION_TRANSFER_BUDGET_PER_REFRESH = 1;
// Bounded pre-original batch: after a burst (morning/wake) one callback may
// process a small number of due bindings back-to-back instead of one per
// callback, but it stops at a hard wall-clock budget so the frame never turns
// into a long freeze.  Each transaction still reserves/releases its own gate
// and keeps its own fresh validation; no pointers cross iterations.
static constexpr unsigned PRODUCTION_DUE_BATCH_MAX_PER_CALLBACK = 4;
static constexpr LONGLONG PRODUCTION_DUE_BATCH_BUDGET_US = 8000;

// ---- Chain automation (position-driven auto-binding) ---------------------
// When a machine is placed within PRODUCTION_AUTOLINK_RADIUS game-units of
// one or more chests, a binding is automatically created/updated.  No panel,
// hotkey or manual config is needed.  One chest  =>  input=output=same chest.
// Two+ chests =>  first adjacent chest is input, second is output.  Auto-linked
// bindings carry the autoLinked flag so ProductionEvaluateBinding can relax the
// same-chest rejection for single-chest mode.
// Game coordinates: 90 units = 1 tile.  Adjacent objects differ by ~90 (same row/column)
// or ~127 (diagonal).  95 covers strict orthogonal 1-tile adjacency (90 + 5 tolerance)
// without linking diagonal or 2+ tile distant objects.  The previous 130 was still
// too large: it linked diagonal neighbors and created phantom groups.
static constexpr float PRODUCTION_AUTOLINK_RADIUS = 95.0f;
static constexpr float PRODUCTION_AUTOLINK_RADIUS_SQ =
    PRODUCTION_AUTOLINK_RADIUS * PRODUCTION_AUTOLINK_RADIUS;
// Periodic auto-scan interval: the chain re-evaluates positions even
// without a panel trigger.  10 seconds balances responsiveness (a newly placed
// chest is picked up within 10 s + 5 s settle) against scan cost (~350 ms).
static constexpr ULONGLONG PRODUCTION_AUTOLINK_SCAN_INTERVAL_MS = 20000;
// v1.1.26: the periodic timer is now a maximum backstop, not the primary
// trigger.  The primary trigger is the declared-count change detector
// (g_productionLastScanDeclaredCount) which fires immediately when the
// player places or removes a gimmick.  The backstop ensures positions are
// re-evaluated at least every 60 s even if nothing changes.
static constexpr ULONGLONG PRODUCTION_AUTOLINK_SCAN_BACKSTOP_MS = 60000;
static std::atomic<ULONGLONG> g_productionAutoLinkLastScanTick{0};
// v1.1.26: tracks the declared gimmick count from the last completed scan.
// The periodic check compares this with the live declared count; if they
// match and the backstop has not elapsed, the scan is skipped entirely.
static std::atomic<size_t> g_productionLastScanDeclaredCount{0};
// A chest must be stationary for this many milliseconds before it is
// included in the flood-fill and becomes eligible for auto-binding.
static constexpr ULONGLONG PRODUCTION_CHEST_SETTLE_MS = 5000;

// Track whether a binding was created by the chain auto-linker.  This field is
// packed into the existing ProductionBinding struct below.
//
// Wall-clock telemetry for the only periodic main-thread work: the due state
// machine.  Integers only (no floating-point varargs in the encoded log),
// throttled, and emitted only when one callback exceeds the threshold so
// steady 60 fps play produces zero diagnostic lines.
static constexpr LONGLONG PRODUCTION_DUE_SLOW_LOG_US = 5000;
static constexpr ULONGLONG PRODUCTION_DUE_SLOW_LOG_INTERVAL_MS = 2000;
static ULONGLONG g_productionLastDueSlowLogTick = 0;

static void ProductionArmTransferCooldown(ULONGLONG delayMs) {
    const ULONGLONG now = GetTickCount64();
    const ULONGLONG target = now > ~static_cast<ULONGLONG>(0) - delayMs
        ? ~static_cast<ULONGLONG>(0) : now + delayMs;
    ULONGLONG observed = g_productionTransferNotBeforeTick.load(
        std::memory_order_acquire);
    for (unsigned attempt = 0; attempt < 64 &&
         observed < target; ++attempt) {
        if (g_productionTransferNotBeforeTick.compare_exchange_weak(
                observed, target, std::memory_order_acq_rel,
                std::memory_order_acquire)) return;
    }
    // A monotonic tick gate never needs an unbounded CAS spin: after the
    // bounded window, publish the later deadline with one store.  This closes
    // the last theoretical main-thread livelock against a concurrent bumper.
    if (target > g_productionTransferNotBeforeTick.load(
            std::memory_order_acquire))
        g_productionTransferNotBeforeTick.store(
            target, std::memory_order_release);
}

// Every binding/master mutation publishes both a time gate and a callback
// epoch while the binding lock is held.  A transaction requires the callback
// which observed the UI edge to finish its original native update before work
// can start in the following callback.
static void ProductionArmBindingMutationTransferGate() {
    ProductionArmTransferCooldown(PRODUCTION_TRANSFER_SETTLE_MS);
    const u64 entered = g_productionMainCallbackEnteredSequence.load(
        std::memory_order_acquire);
    u64 observed = g_productionTransferMutationSequence.load(
        std::memory_order_acquire);
    for (unsigned attempt = 0; attempt < 64 && observed < entered; ++attempt) {
        if (g_productionTransferMutationSequence.compare_exchange_weak(
                observed, entered, std::memory_order_acq_rel,
                std::memory_order_acquire)) break;
    }
    if (entered > g_productionTransferMutationSequence.load(
            std::memory_order_acquire))
        g_productionTransferMutationSequence.store(
            entered, std::memory_order_release);
    g_productionMutationDeferredLogged.store(false,
                                              std::memory_order_release);
}

static bool ProductionModalInputIsBlocked() {
    return (g_productionModalToken.load(std::memory_order_acquire) & 0x3u) !=
        static_cast<u64>(ProductionModalState::Hidden);
}

static u64 ProductionModalGeneration(u64 token) {
    return token >> 4;
}

static ProductionModalState ProductionModalTokenState(u64 token) {
    return static_cast<ProductionModalState>(token & 0x3u);
}

static unsigned ProductionModalTokenMask(u64 token) {
    return static_cast<unsigned>((token >> 2) & 0x3u);
}

static u64 ProductionPackModalToken(u64 generation,
                                    ProductionModalState state,
                                    unsigned edgeMask) {
    return (generation << 4) |
           ((static_cast<u64>(edgeMask) & 0x3u) << 2) |
           static_cast<u64>(state);
}

static u64 ProductionAcquireModalInputBlock(unsigned openingMask,
                                            HWND exactOpeningOwner) {
    if (!exactOpeningOwner || !ProductionModalApiIsolationIsReady()) return 0;
    AcquireSRWLockExclusive(&g_productionModalAcquireLock);
    u64 acquiredGeneration = 0;
    u64 observed = g_productionModalToken.load(std::memory_order_acquire);
    for (;;) {
        if (ProductionModalTokenState(observed) !=
            ProductionModalState::Hidden) break;
        const u64 generation = ProductionModalGeneration(observed) + 1;
        const u64 desired = ProductionPackModalToken(
            generation, ProductionModalState::Opening, openingMask);
        // Publish the immutable owner association before the packed token.
        // A worker can therefore never observe Opening without a matching
        // generation-bound owner; stale associations are ignored by generation.
        g_productionModalOpeningOwner.store(exactOpeningOwner,
                                             std::memory_order_relaxed);
        g_productionModalOpeningOwnerGeneration.store(
            generation, std::memory_order_release);
        if (g_productionModalToken.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            g_productionModalDrainRequiredCallback.store(
                0, std::memory_order_release);
            g_productionModalDrainCompletedGeneration.store(
                0, std::memory_order_release);
            acquiredGeneration = generation;
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_productionModalAcquireLock);
    return acquiredGeneration;
}

// Ordinary worker/UI teardown is generation-exact.  Generation zero is a real
// initial token, never a wildcard: otherwise a worker that sampled the initial
// Hidden token could clear the first Opening generation published immediately
// afterwards.
static bool ProductionReleaseModalInputBlock(u64 expectedGeneration) {
    u64 observed = g_productionModalToken.load(std::memory_order_acquire);
    for (;;) {
        const u64 generation = ProductionModalGeneration(observed);
        if (generation != expectedGeneration) return false;
        if (ProductionModalTokenState(observed) == ProductionModalState::Hidden)
            return true;
        const u64 desired = ProductionPackModalToken(
            generation, ProductionModalState::Hidden, 0);
        if (g_productionModalToken.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel,
                std::memory_order_acquire)) return true;
    }
}

// Main-thread world lifecycle and the serialized master-disable edge are
// authoritative cancellation points.  They intentionally revoke whichever
// generation is current, using one packed-token CAS rather than a split
// generation check/state store.
static bool ProductionForceReleaseModalInputBlock() {
    u64 observed = g_productionModalToken.load(std::memory_order_acquire);
    for (;;) {
        if (ProductionModalTokenState(observed) == ProductionModalState::Hidden)
            return true;
        const u64 desired = ProductionPackModalToken(
            ProductionModalGeneration(observed), ProductionModalState::Hidden,
            0);
        if (g_productionModalToken.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel,
                std::memory_order_acquire)) return true;
    }
}

static bool ProductionTransitionModalInputBlock(
        u64 generation, ProductionModalState expectedState,
        ProductionModalState nextState, unsigned edgeMask) {
    u64 observed = g_productionModalToken.load(std::memory_order_acquire);
    for (;;) {
        if (ProductionModalGeneration(observed) != generation ||
            ProductionModalTokenState(observed) != expectedState) return false;
        const u64 desired = ProductionPackModalToken(
            generation, nextState, edgeMask);
        if (g_productionModalToken.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel,
                std::memory_order_acquire)) return true;
    }
}

static u64 ProductionBeginMainCallback() {
    ++g_productionMainCallbackDepth;
    if (g_productionMainCallbackDepth != 1) return 0;
    const u64 sequence = g_productionMainCallbackEnteredSequence.fetch_add(
        1, std::memory_order_acq_rel) + 1;
    g_productionMainCallbackStartedAt = GetTickCount64();
    g_productionMainCallbackEnteredTick.store(
        g_productionMainCallbackStartedAt, std::memory_order_release);
    g_productionMainCallbackPhase.store(1, std::memory_order_release);
    g_productionMainCallbackInFlight.store(true, std::memory_order_release);
    return sequence;
}

static void ProductionSetMainCallbackPhase(unsigned phase) {
    if (g_productionMainCallbackDepth == 1)
        g_productionMainCallbackPhase.store(phase, std::memory_order_release);
}

static void ProductionCompleteMainCallback(u64 callbackSequence) {
    if (g_productionMainCallbackDepth == 0) return;
    if (g_productionMainCallbackDepth != 1 || callbackSequence == 0) {
        --g_productionMainCallbackDepth;
        return;
    }
    const ULONGLONG completedAt = GetTickCount64();
    const ULONGLONG duration = completedAt >= g_productionMainCallbackStartedAt
        ? completedAt - g_productionMainCallbackStartedAt : 0;
    g_productionMainCallbackDurationMs.store(duration,
                                              std::memory_order_release);
    g_productionMainCallbackCompletedTick.store(completedAt,
                                                 std::memory_order_release);
    g_productionMainCallbackCompletedSequence.store(
        callbackSequence, std::memory_order_release);
    const u64 modalToken = g_productionModalToken.load(
        std::memory_order_acquire);
    const u64 modalDrainRequired =
        g_productionModalDrainRequiredCallback.load(
            std::memory_order_acquire);
    if (ProductionModalTokenState(modalToken) ==
            ProductionModalState::ClosingDrain &&
        modalDrainRequired != 0 &&
        callbackSequence >= modalDrainRequired) {
        g_productionModalDrainCompletedGeneration.store(
            ProductionModalGeneration(modalToken),
            std::memory_order_release);
    }
    g_productionPreUpdateStage.store(0, std::memory_order_release);
    g_productionMainCallbackPhase.store(0, std::memory_order_release);
    g_productionMainCallbackInFlight.store(false, std::memory_order_release);
    --g_productionMainCallbackDepth;
}

static bool ProductionNaturalMorningEdgeIsVerified(u64 generation) {
    if (generation == 0 ||
        generation == g_productionNaturalMorningGenerationObserved ||
        g_autoPetMorningReadyGeneration.load(std::memory_order_acquire) !=
            generation) return false;
    const u64 entered = g_productionMainCallbackEnteredSequence.load(
        std::memory_order_acquire);
    const u64 completed = g_productionMainCallbackCompletedSequence.load(
        std::memory_order_acquire);
    return entered != 0 && completed != UINT64_MAX &&
        completed + 1 == entered;
}

static void ProductionObserveWorldContext(
        bool valid, u64 naturalMorningGeneration) {
    if (valid) {
        if (!g_productionWorldContextActive.exchange(
                true, std::memory_order_acq_rel)) {
            bool naturalMorningEdge =
                naturalMorningGeneration != 0 &&
                g_productionNaturalMorningEdgePending ==
                    naturalMorningGeneration;
            if (!naturalMorningEdge &&
                ProductionNaturalMorningEdgeIsVerified(
                    naturalMorningGeneration)) {
                naturalMorningEdge = true;
                g_productionNaturalMorningGenerationObserved =
                    naturalMorningGeneration;
            }
            g_productionNaturalMorningEdgePending = 0;
            // A verified natural morning already completed the native reload
            // callback which published AutoPet's ready generation.  It still
            // receives a fresh value epoch and must wait for an exact clock
            // snapshot from this new epoch, but it is not a save/world load and
            // therefore does not inherit the generic five-second wall gate.
            g_productionWorldActiveSince = GetTickCount64();
            g_productionTransferDeferredLogged = false;
            g_productionNaturalMorningSettleReady = naturalMorningEdge;
            if (!naturalMorningEdge)
                ProductionArmTransferCooldown(PRODUCTION_TRANSFER_SETTLE_MS);
            g_productionWorldScheduleEpoch.fetch_add(
                1, std::memory_order_acq_rel);
        }
        g_productionLastWorldCallbackTick.store(GetTickCount64(),
                                                std::memory_order_release);
        return;
    }

    const bool pendingNaturalMorningEdge =
        naturalMorningGeneration != 0 &&
        g_productionNaturalMorningEdgePending == naturalMorningGeneration &&
        g_productionNaturalMorningGenerationObserved ==
            naturalMorningGeneration;
    const bool newlyVerifiedNaturalMorningEdge =
        ProductionNaturalMorningEdgeIsVerified(naturalMorningGeneration);
    const bool naturalMorningEdge = pendingNaturalMorningEdge ||
        newlyVerifiedNaturalMorningEdge;
    if (newlyVerifiedNaturalMorningEdge) {
        g_productionNaturalMorningGenerationObserved =
            naturalMorningGeneration;
        g_productionNaturalMorningEdgePending = naturalMorningGeneration;
    } else if (!pendingNaturalMorningEdge) {
        g_productionNaturalMorningEdgePending = 0;
    }
    g_productionNaturalMorningSettleReady = false;

    // This is atomics-only and is safe on the game main thread.  Window/capture
    // teardown remains worker-owned, but native input must be released in the
    // same callback that observes map/load/world loss.
    ProductionForceReleaseModalInputBlock();

    // Context loss and the master-switch off edge both arrive through the
    // shared main callback.  Make the teardown idempotent so a disabled feature
    // does not publish a new empty generation on every frame.
    const bool wasActive = g_productionWorldContextActive.exchange(
        false, std::memory_order_acq_rel);
    if (wasActive)
        g_productionWorldScheduleEpoch.fetch_add(
            1, std::memory_order_acq_rel);
    if (!wasActive &&
        g_productionLastWorldCallbackTick.load(std::memory_order_acquire) == 0 &&
        !g_productionScanInProgress.load(std::memory_order_acquire) &&
        !g_productionRegistryScanRequested.load(std::memory_order_acquire)) {
        return;
    }

    g_productionLastWorldCallbackTick.store(0, std::memory_order_release);
    g_productionPreUpdateStage.store(0, std::memory_order_release);
    g_productionRegistryScanRequested.store(false, std::memory_order_release);
    g_productionScanInProgress.store(false, std::memory_order_release);
    g_productionScanOwnerThread.store(0, std::memory_order_release);
    g_productionWorldActiveSince = 0;
    g_productionTransferDeferredLogged = false;
    if (!naturalMorningEdge)
        ProductionArmTransferCooldown(PRODUCTION_TRANSFER_SETTLE_MS);
    ProductionSetActiveMachineId({});

    // The thread-local cursor never survives a lifecycle boundary.  Only
    // fixed-size Mod values are published; no registry node/status pointer is
    // retained for a later callback.
    g_productionScanActive = false;
    g_productionScanCursor = 0;
    g_productionScanExpected = 0;
    g_productionScanPrefixHash = 1469598103934665603ULL;
    g_productionScanResumeNode = nullptr;
    g_productionScanResumePrev = nullptr;
    g_productionScanning = false;
    AcquireSRWLockExclusive(&g_productionPublishLock);
    g_productionPublished.generation =
        g_productionSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    g_productionPublished.activeMachine = {};
    g_productionPublished.activeMachinePresent = false;
    g_productionPublished.scanActive = false;
    g_productionPublished.scanProcessed = 0;
    g_productionPublished.scanTotal = 0;
    g_productionPublished.chestCount = 0;
    ReleaseSRWLockExclusive(&g_productionPublishLock);
}

// Sequence allocation and the physical write share this lock.  A smaller
// sequence can therefore never be delayed behind a later worker-thread line.
template<EncodedLogFormat encoded, typename... Args>
static void ProductionSecureLog(Args... args) {
    AcquireSRWLockExclusive(&g_productionLogLock);
    const u64 sequence = ProductionNextLogSequence();
    SecureLog<encoded>(static_cast<unsigned long long>(sequence), args...);
    ReleaseSRWLockExclusive(&g_productionLogLock);
}

#ifdef PRODUCTIONAUTO_LOGGING
#define ProductionLog(format, ...) \
    ProductionSecureLog<EncodedLogFormat{format}>(__VA_ARGS__)
#else
#define ProductionLog(format, ...) ((void)0)
#endif

static LONGLONG ProductionTicksToMicroseconds(LONGLONG ticks) {
    if (g_nearbyPerformanceFrequency <= 0 || ticks < 0) return -1;
    return (ticks * 1000000LL) / g_nearbyPerformanceFrequency;
}

// Cross-callback readable-region reuse for the due scheduler only.  The cache
// stores committed address ranges, never object pointers.  It is invalidated
// on world/load edges and whenever the registry node count changes; a stale
// range is therefore never used after a save transition or object add/remove
// that could free heap pages.
static NearbyReadableRegionCache g_productionPersistentReadableCache = {};
static size_t g_productionPersistentReadableDeclared = SIZE_MAX;
static u64 g_productionPersistentReadableLoadGeneration = 0;
static void ProductionResetFallbackAttemptDiagnostics();

static void ProductionInvalidatePersistentReadableCache() {
    g_productionPersistentReadableCache.count = 0;
    g_productionPersistentReadableCache.lastHit = 0;
    g_productionPersistentReadableDeclared = SIZE_MAX;
    g_productionPersistentReadableLoadGeneration = 0;
    ProductionResetFallbackAttemptDiagnostics();
}

static void ProductionEnsurePersistentReadableCache(
        NearbyReadableRegionCache* cache, size_t declared,
        u64 loadGeneration) {
    if (!cache) return;
    if (g_productionPersistentReadableLoadGeneration != loadGeneration ||
        g_productionPersistentReadableDeclared != declared) {
        g_productionPersistentReadableCache.count = 0;
        g_productionPersistentReadableCache.lastHit = 0;
        cache->count = 0;
        cache->lastHit = 0;
        g_productionPersistentReadableDeclared = declared;
        g_productionPersistentReadableLoadGeneration = loadGeneration;
    }
}

static void ProductionStorePersistentReadableCache(
        const NearbyReadableRegionCache& cache) {
    g_productionPersistentReadableCache = cache;
}

struct ProductionResolveDiagnostics {
    LONGLONG registryBegin;
    LONGLONG registryEnd;
    LONGLONG objectBegin;
    LONGLONG objectEnd;
    size_t identityReads;
    size_t walkedNodes;
    size_t fallbackAttempt;
    size_t registryCachePublishEntries;
    bool fallbackWalk;
    bool fallbackFirst;
};
static thread_local ProductionResolveDiagnostics g_productionResolveDiag = {};
// Scalar-only diagnostic generation.  It distinguishes the expected one-time
// ordinal-index build for a registry shape from an unexpected repeat without
// retaining a node/status pointer across callbacks.
static thread_local u64 g_productionFallbackAttemptLoadGeneration = ~0ULL;
static thread_local size_t g_productionFallbackAttemptDeclared = SIZE_MAX;
static thread_local size_t g_productionFallbackAttemptCount = 0;

static void ProductionResetFallbackAttemptDiagnostics() {
    g_productionFallbackAttemptLoadGeneration = ~0ULL;
    g_productionFallbackAttemptDeclared = SIZE_MAX;
    g_productionFallbackAttemptCount = 0;
}

struct ProductionDueTimingScope {
    LONGLONG begin;
    LONGLONG selected;
    LONGLONG resolved;
    LONGLONG attempted;
    u64 callbackSequence;
    std::int64_t gameSecond;
    NearbyReadableRegionCache* readableCache;

    explicit ProductionDueTimingScope(
            u64 sequence, NearbyReadableRegionCache* cache) : begin(0),
        selected(0), resolved(0), attempted(0),
        callbackSequence(sequence), gameSecond(0),
        readableCache(cache) {
        begin = NearbyPerformanceCounter();
    }

    ~ProductionDueTimingScope() {
        const LONGLONG end = NearbyPerformanceCounter();
        if (readableCache)
            ProductionStorePersistentReadableCache(*readableCache);
        const LONGLONG totalUs = ProductionTicksToMicroseconds(end - begin);
        if (totalUs <= PRODUCTION_DUE_SLOW_LOG_US) return;
        const ULONGLONG now = GetTickCount64();
        if (now - g_productionLastDueSlowLogTick <
            PRODUCTION_DUE_SLOW_LOG_INTERVAL_MS) return;
        g_productionLastDueSlowLogTick = now;
        const LONGLONG registryUs =
            g_productionResolveDiag.registryBegin &&
            g_productionResolveDiag.registryEnd
            ? ProductionTicksToMicroseconds(
                  g_productionResolveDiag.registryEnd -
                  g_productionResolveDiag.registryBegin) : -1;
        const LONGLONG objectUs =
            g_productionResolveDiag.objectBegin &&
            g_productionResolveDiag.objectEnd
            ? ProductionTicksToMicroseconds(
                  g_productionResolveDiag.objectEnd -
                  g_productionResolveDiag.objectBegin) : -1;
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=due_slow "
            "result=INCOMPLETE reason=due_state_machine_over_threshold "
            "callback_sequence=%llu total_us=%lld select_us=%lld "
            "resolve_us=%lld attempt_us=%lld game_second=%lld "
            "fallback_walk=%d fallback_class=%s fallback_attempt=%zu "
            "cache_publish_entries=%zu walked_nodes=%zu identity_reads=%zu "
            "registry_us=%lld object_us=%lld "
            "virtual_queries=%llu read_checks=%llu cache_hits=%llu "
            "raw_pointer_cache=0 writes=0\n",
            static_cast<unsigned long long>(callbackSequence),
            static_cast<long long>(totalUs),
            static_cast<long long>(selected
                ? ProductionTicksToMicroseconds(selected - begin) : -1),
            static_cast<long long>(resolved
                ? ProductionTicksToMicroseconds(resolved - selected) : -1),
            static_cast<long long>(attempted
                ? ProductionTicksToMicroseconds(attempted - resolved) : -1),
            static_cast<long long>(gameSecond),
            g_productionResolveDiag.fallbackWalk ? 1 : 0,
            !g_productionResolveDiag.fallbackWalk ? "cached_revalidation" :
                (g_productionResolveDiag.fallbackFirst ? "first_rebuild" :
                 "repeated_fallback"),
            g_productionResolveDiag.fallbackAttempt,
            g_productionResolveDiag.registryCachePublishEntries,
            g_productionResolveDiag.walkedNodes,
            g_productionResolveDiag.identityReads,
            static_cast<long long>(registryUs),
            static_cast<long long>(objectUs),
            static_cast<unsigned long long>(readableCache
                ? readableCache->virtualQueries : 0),
            static_cast<unsigned long long>(readableCache
                ? readableCache->checks : 0),
            static_cast<unsigned long long>(readableCache
                ? readableCache->cacheHits : 0));
    }

    ProductionDueTimingScope(const ProductionDueTimingScope&) = delete;
    ProductionDueTimingScope& operator=(
        const ProductionDueTimingScope&) = delete;
};

using ProductionResolveComponentFunction = void* (__fastcall *)(void*);
static ProductionResolveComponentFunction g_productionResolveComponent = nullptr;

static const char* ProductionStateName(ProductionState state) {
    switch (state) {
    case ProductionState::Unbound: return "UNBOUND";
    case ProductionState::Idle: return "IDLE";
    case ProductionState::WaitingInput: return "WAITING_INPUT";
    case ProductionState::Starting: return "STARTING";
    case ProductionState::Running: return "RUNNING";
    case ProductionState::OutputReady: return "OUTPUT_READY";
    case ProductionState::WaitingOutputSpace: return "WAITING_OUTPUT_SPACE";
    case ProductionState::Faulted: return "FAULTED";
    }
    return "FAULTED";
}

static const char* ProductionReasonName(ProductionReason reason) {
    switch (reason) {
    case ProductionReason::None: return "none";
    case ProductionReason::InputUnbound: return "input_unbound";
    case ProductionReason::OutputUnbound: return "output_unbound";
    case ProductionReason::PausedByUser: return "paused_by_user";
    case ProductionReason::WaitingIngredients: return "waiting_ingredients";
    case ProductionReason::RunningNativeTimer: return "running_native_timer";
    case ProductionReason::OutputNeedsTransfer: return "output_needs_transfer";
    case ProductionReason::OutputCapacityInsufficient: return "output_capacity_insufficient";
    case ProductionReason::ChestNameInvalid: return "chest_name_invalid";
    case ProductionReason::ChestIdentityMissing: return "chest_identity_missing";
    case ProductionReason::MachineIdentityMissing: return "machine_identity_missing";
    case ProductionReason::AmbiguousIdentity: return "ambiguous_identity";
    case ProductionReason::InvalidObjectType: return "invalid_object_type";
    case ProductionReason::SameChestRejected: return "same_chest_rejected";
    case ProductionReason::NativeTransactionEvidenceRequired:
        return "native_transaction_evidence_required";
    case ProductionReason::RegistryInvalid: return "registry_invalid";
    case ProductionReason::MainThreadChanged: return "main_thread_changed";
    case ProductionReason::ConfigCorrupt: return "config_corrupt";
    case ProductionReason::BindingSettlePending:
        return "binding_settle_pending";
    }
    return "unknown";
}

// Chinese display names are panel/marker-only.  Log contracts keep the
// English names above; monitors validate those exact tokens.
static const wchar_t* ProductionStateNameZh(ProductionState state) {
    switch (state) {
    case ProductionState::Unbound: return L"未绑定";
    case ProductionState::Idle: return L"空闲(已暂停)";
    case ProductionState::WaitingInput: return L"等待原料";
    case ProductionState::Starting: return L"启动中";
    case ProductionState::Running: return L"运行中";
    case ProductionState::OutputReady: return L"产出就绪";
    case ProductionState::WaitingOutputSpace: return L"等待输出空间";
    case ProductionState::Faulted: return L"故障";
    }
    return L"故障";
}

static const wchar_t* ProductionReasonNameZh(ProductionReason reason) {
    switch (reason) {
    case ProductionReason::None: return L"无";
    case ProductionReason::InputUnbound: return L"未绑定输入箱";
    case ProductionReason::OutputUnbound: return L"未绑定输出箱";
    case ProductionReason::PausedByUser: return L"用户已暂停";
    case ProductionReason::WaitingIngredients: return L"等待原料";
    case ProductionReason::RunningNativeTimer: return L"原生计时进行中";
    case ProductionReason::OutputNeedsTransfer: return L"产出待转运";
    case ProductionReason::OutputCapacityInsufficient: return L"输出箱空间不足";
    case ProductionReason::ChestNameInvalid: return L"箱子未命名";
    case ProductionReason::ChestIdentityMissing: return L"箱子不存在";
    case ProductionReason::MachineIdentityMissing: return L"设备不存在";
    case ProductionReason::AmbiguousIdentity: return L"身份歧义";
    case ProductionReason::InvalidObjectType: return L"对象类型不符";
    case ProductionReason::SameChestRejected: return L"输入输出箱相同";
    case ProductionReason::NativeTransactionEvidenceRequired:
        return L"等待原生事务证据";
    case ProductionReason::RegistryInvalid: return L"注册表无效";
    case ProductionReason::MainThreadChanged: return L"主线程变更";
    case ProductionReason::ConfigCorrupt: return L"配置损坏";
    case ProductionReason::BindingSettlePending: return L"等待安全刷新";
    }
    return L"未知";
}
static COLORREF ProductionStateAccent(ProductionState state, bool enabled) {
    switch (state) {
    case ProductionState::Running:
    case ProductionState::OutputReady:
        return enabled ? RGB(80, 205, 118) : RGB(244, 186, 84);
    case ProductionState::Idle:
    case ProductionState::WaitingInput:
    case ProductionState::Starting:
    case ProductionState::WaitingOutputSpace:
        return enabled ? RGB(94, 158, 235) : RGB(244, 186, 84);
    case ProductionState::Unbound:
    case ProductionState::Faulted:
    default:
        return RGB(231, 98, 98);
    }
}

static bool ProductionBuildSealValid() {
    u32 digest = 0x811c9dc5u;
    for (size_t index = 0; index < sizeof(g_productionAutomationBuildSeal);
         ++index) {
        digest ^= static_cast<u32>(g_productionAutomationBuildSeal[index]);
        digest *= 0x01000193u;
    }
    return digest == PRODUCTION_AUTOMATION_BUILD_SEAL_FNV1A;
}

static bool ProductionReadPointer(const void* object, uintptr_t offset,
                                  void** output) {
    if (!object || !output) return false;
    const unsigned char* field =
        reinterpret_cast<const unsigned char*>(object) + offset;
    if (!NearbyIsReadable(field, sizeof(void*))) return false;
    *output = *reinterpret_cast<void* const*>(field);
    return *output != nullptr;
}

static bool ProductionReadCString(const char* source, char* output,
                                  size_t outputSize) {
    if (!source || !output || outputSize < 2) return false;
    size_t length = 0;
    for (; length + 1 < outputSize; ++length) {
        if (!NearbyIsReadable(source + length, 1)) return false;
        const char value = source[length];
        output[length] = value;
        if (value == '\0') return true;
    }
    output[0] = '\0';
    return false;
}

static bool ProductionNameHasVisibleUtf8(const char* text, size_t length) {
    if (!text || length == 0 || length > PRODUCTION_MAX_NAME_BYTES)
        return false;
    wchar_t wide[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    const int converted = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text, static_cast<int>(length), wide,
        static_cast<int>(sizeof(wide) / sizeof(wide[0]) - 1));
    if (converted <= 0) return false;
    for (int index = 0; index < converted; ++index) {
        WORD type = 0;
        if (!GetStringTypeW(CT_CTYPE1, wide + index, 1, &type)) return false;
        if ((type & C1_SPACE) == 0) return true;
    }
    return false;
}

static bool ProductionReadStatusMapValue(void* status, uintptr_t mapOffset,
                                         const char* key, u64* output,
                                         bool valueIsInt) {
    if (!status || !key || !output || strlen(key) > 8 ||
        !NearbyIsReadable(reinterpret_cast<unsigned char*>(status) + mapOffset,
                    0x38)) return false;
    u64 packedKey = 0;
    memcpy(&packedKey, key, strlen(key));
    unsigned char* map = reinterpret_cast<unsigned char*>(status) + mapOffset;
    void* sentinel = *reinterpret_cast<void**>(map + 0x08);
    const u64 size = *reinterpret_cast<const u64*>(map + 0x10);
    void* buckets = *reinterpret_cast<void**>(map + 0x18);
    const u64 mask = *reinterpret_cast<const u64*>(map + 0x30);
    if (!sentinel || !NearbyIsReadable(sentinel, 0x20) || size > 4096 ||
        mask > 4095 || ((mask + 1) & mask) != 0 || !buckets ||
        !NearbyIsReadable(buckets, static_cast<size_t>(mask + 1) * 16)) return false;
    void** pair = reinterpret_cast<void**>(
        reinterpret_cast<unsigned char*>(buckets) +
        (packedKey & mask) * 16);
    void* stop = pair[0];
    void* node = pair[1];
    if ((node == sentinel) != (stop == sentinel)) return false;
    size_t visited = 0;
    while (node != sentinel && visited++ <= static_cast<size_t>(size)) {
        if (!node || !NearbyIsReadable(node, 0x20)) return false;
        if (*reinterpret_cast<const u64*>(
                reinterpret_cast<unsigned char*>(node) + 0x10) == packedKey) {
            *output = valueIsInt
                ? static_cast<u64>(static_cast<std::int64_t>(
                      *reinterpret_cast<const int*>(
                          reinterpret_cast<unsigned char*>(node) + 0x18)))
                : *reinterpret_cast<const u64*>(
                      reinterpret_cast<unsigned char*>(node) + 0x18);
            return true;
        }
        if (node == stop) break;
        node = *reinterpret_cast<void**>(
            reinterpret_cast<unsigned char*>(node) + sizeof(void*));
    }
    *output = 0;
    return true; // a verified miss has the same zero value as the Lua getter
}

static bool ProductionReadMapId(void* status, u64* mapId) {
    if (!status || !mapId) return false;
    void* holder = nullptr;
    void* group = nullptr;
    if (!ProductionReadPointer(status, PRODUCTION_MAP_HOLDER_OFFSET, &holder) ||
        !ProductionReadPointer(holder, 0, &group) ||
        !NearbyIsReadable(group, sizeof(u64))) return false;
    const u64 value = *reinterpret_cast<const u64*>(group);
    if (value == 0) return false;
    *mapId = value;
    return true;
}

static bool ProductionReadGimmickIdentity(void* status,
                                          ProductionStableId* identity) {
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!status || !identity ||
        !NearbyIsReadable(status, PRODUCTION_UNIQUE_NAME_POINTER_OFFSET +
                                sizeof(void*) * 2) ||
        *reinterpret_cast<const uintptr_t*>(status) !=
            base + RVA_PRODUCTION_GIMMICK_VTABLE ||
        *reinterpret_cast<const uintptr_t*>(
             reinterpret_cast<const unsigned char*>(status) + 0x10) !=
            base + RVA_PRODUCTION_GIMMICK_SECONDARY_VTABLE) return false;
    const u64 uniqueId = *reinterpret_cast<const u64*>(
        reinterpret_cast<const unsigned char*>(status) +
        PRODUCTION_UNIQUE_ID_OFFSET);
    u64 mapId = 0;
    if (!uniqueId || !ProductionReadMapId(status, &mapId)) return false;
    *identity = {mapId, uniqueId};
    return true;
}

static u64 ProductionExtendStableIdentityHash(
        u64 hash, const ProductionStableId& identity) {
    const u64 words[2] = {identity.mapId, identity.uniqueId};
    const unsigned char* bytes =
        reinterpret_cast<const unsigned char*>(words);
    for (size_t index = 0; index < sizeof(words); ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// Read only the gimmickId (first 8 bytes of data) without the module
// string.  This is the fast-path filter: 1 ProductionReadPointer +
// 1 NearbyIsReadable + 1 dereference.  Use ProductionIsMachineId to
// skip ~90% of registry objects that are neither machines nor (from
// the ID alone) distinguishable as chests/floors.
static bool ProductionReadGimmickIdOnly(void* status, u64* gimmickId) {
    void* holder = nullptr;
    void* data = nullptr;
    if (!status || !gimmickId ||
        !ProductionReadPointer(status, PRODUCTION_GIMMICK_DATA_OFFSET, &holder) ||
        !ProductionReadPointer(holder, 0, &data) ||
        !NearbyIsReadable(data, sizeof(u64))) return false;
    const u64 id = *reinterpret_cast<const u64*>(data);
    if (!id) return false;
    *gimmickId = id;
    return true;
}

// Read the module string from the gimmick data block.  Assumes the
// caller already has a valid data pointer or uses the internal
// holder→data indirection.
static bool ProductionReadGimmickModule(void* status, char* module,
                                        size_t moduleSize) {
    void* holder = nullptr;
    void* data = nullptr;
    if (!status || !module || moduleSize < 2 ||
        !ProductionReadPointer(status, PRODUCTION_GIMMICK_DATA_OFFSET, &holder) ||
        !ProductionReadPointer(holder, 0, &data) ||
        !NearbyIsReadable(data, GIMMICK_MODULE_NAME_OFFSET + sizeof(void*)))
        return false;
    const char* modulePointer = *reinterpret_cast<const char* const*>(
        reinterpret_cast<const unsigned char*>(data) +
        GIMMICK_MODULE_NAME_OFFSET);
    return ProductionReadCString(modulePointer, module, moduleSize);
}

static bool ProductionReadGimmickType(void* status, u64* gimmickId,
                                      char* module, size_t moduleSize) {
    void* holder = nullptr;
    void* data = nullptr;
    if (!status || !gimmickId || !module || moduleSize < 2 ||
        !ProductionReadPointer(status, PRODUCTION_GIMMICK_DATA_OFFSET, &holder) ||
        !ProductionReadPointer(holder, 0, &data) ||
        !NearbyIsReadable(data, GIMMICK_MODULE_NAME_OFFSET + sizeof(void*))) return false;
    const u64 id = *reinterpret_cast<const u64*>(data);
    const char* modulePointer = *reinterpret_cast<const char* const*>(
        reinterpret_cast<const unsigned char*>(data) +
        GIMMICK_MODULE_NAME_OFFSET);
    if (!id || !ProductionReadCString(modulePointer, module, moduleSize))
        return false;
    *gimmickId = id;
    return true;
}

static bool ProductionIsMachineId(u64 id) {
    switch (id) {
    case 240010000ULL: case 240010100ULL: // storage/pickle jar
    case 240020000ULL: case 240020100ULL: // milk processing machine
    case 240030000ULL: case 240030100ULL: // mayonnaise machine
    case 240040000ULL: case 240040100ULL: // brewing barrel
    case 240050000ULL: case 240050100ULL: // aging barrel
    case 240060000ULL: case 240060100ULL: // drying machine
    case 240070000ULL: case 240070100ULL: // flour mill
    case 240080000ULL: case 240080100ULL: // oil extraction machine
    case 240090000ULL: case 240090100ULL: // blast furnace
    case 240100000ULL: case 240100100ULL: // charcoal kiln
    case 240110000ULL: case 240110100ULL: // loom
    case 240120000ULL: case 240120100ULL: // composter
    case 240150000ULL: case 240150100ULL: // smoking machine
    case 240240000ULL:                    // beehive
    case 240250000ULL:                    // sap extractor
    case 240270000ULL:                    // sericulture box
        return true;
    default:
        return false;
    }
}

static bool ProductionIsMachineType(u64 id, const char* module) {
    if (!module || !ProductionIsMachineId(id)) return false;
    switch (id) {
    // Pickle jars (gimmick_storage_jar / Lv2) are processing machines
    // with 152 recipes, not pure containers.
    case 240010000ULL: return strcmp(module, "gimmick_storage_jar") == 0;
    case 240010100ULL: return strcmp(module, "gimmick_storage_jar_Lv2") == 0;
    case 240020000ULL: return strcmp(module, "gimmick_milk_processing_machine") == 0;
    case 240020100ULL: return strcmp(module, "gimmick_milk_processing_machine_Lv2") == 0;
    case 240030000ULL: return strcmp(module, "gimmick_mayonnaise_processing_machine") == 0;
    case 240030100ULL: return strcmp(module, "gimmick_mayonnaise_processing_machine_Lv2") == 0;
    case 240040000ULL: return strcmp(module, "gimmick_brewing_barrel") == 0;
    case 240040100ULL: return strcmp(module, "gimmick_brewing_barrel_Lv2") == 0;
    case 240050000ULL: return strcmp(module, "gimmick_agung_barrel") == 0;
    case 240050100ULL: return strcmp(module, "gimmick_agung_barrel_Lv2") == 0;
    case 240060000ULL: return strcmp(module, "gimmick_drying_machine") == 0;
    case 240060100ULL: return strcmp(module, "gimmick_drying_machine_Lv2") == 0;
    case 240070000ULL: return strcmp(module, "gimmick_flour_mill") == 0;
    case 240070100ULL: return strcmp(module, "gimmick_flour_mill_Lv2") == 0;
    case 240080000ULL: return strcmp(module, "gimmick_oil_extraction_machine") == 0;
    case 240080100ULL: return strcmp(module, "gimmick_oil_extraction_machine_Lv2") == 0;
    case 240090000ULL: return strcmp(module, "gimmick_blast_furnace") == 0;
    case 240090100ULL: return strcmp(module, "gimmick_blast_furnace_Lv2") == 0;
    case 240100000ULL: return strcmp(module, "gimmick_charcoal_kiln") == 0;
    case 240100100ULL: return strcmp(module, "gimmick_charcoal_kiln_Lv2") == 0;
    case 240110000ULL: return strcmp(module, "gimmick_loom") == 0;
    case 240110100ULL: return strcmp(module, "gimmick_loom_Lv2") == 0;
    case 240120000ULL: return strcmp(module, "gimmick_composter") == 0;
    case 240120100ULL: return strcmp(module, "gimmick_composter_Lv2") == 0;
    case 240150000ULL: return strcmp(module, "gimmick_smoking_machine") == 0;
    case 240150100ULL: return strcmp(module, "gimmick_smoking_machine_Lv2") == 0;
    case 240240000ULL: return strcmp(module, "gimmick_beehive") == 0;
    case 240250000ULL: return strcmp(module, "gimmick_sap_extractor") == 0;
    case 240270000ULL: return strcmp(module, "gimmick_sericulture_box") == 0;
    default: return false;
    }
}

// Passive-production machines (beehive, sap extractor, sericulture box)
// may not use the standard start/duration timer fields.  For these, output
// ready detection falls back to checking itemID/itemVal directly.
static bool ProductionIsPassiveMachine(u64 gimmickId) {
    return gimmickId == 240240000ULL ||  // beehive
           gimmickId == 240250000ULL ||  // sap extractor
           gimmickId == 240270000ULL;    // sericulture box
}

// Check whether a machine slot has output (itemID + itemVal > 0) without
// relying on start/duration timers.  Returns true if the slot has a
// producible item.  Used for passive machines whose timers are always 0.
static bool ProductionSlotHasOutputNoTimer(
        void* machineStatus, size_t slot) {
    if (!machineStatus || slot >= PRODUCTION_MACHINE_SLOTS) return false;
    char idKey[8] = {};
    char countKey[9] = {};
    _snprintf_s(idKey, sizeof(idKey), _TRUNCATE, "itemID%zu", slot);
    _snprintf_s(countKey, sizeof(countKey), _TRUNCATE, "itemVal%zu", slot);
    u64 itemId = 0;
    u64 countValue = 0;
    if (!ProductionReadStatusMapValue(machineStatus, 0x58, idKey, &itemId,
                                      false) ||
        !ProductionReadStatusMapValue(machineStatus, 0x18, countKey,
                                      &countValue, true))
        return false;
    return itemId != 0 && countValue > 0;
}

static bool ProductionReadMachineSlotLimit(void* machineStatus,
                                           size_t* limitOut,
                                           void** dataOut = nullptr);

// Check whether a module name represents a floor tile placed by the
// player.  These are bridge nodes in the flood-fill: they connect
// machines and chests that are farther apart than PRODUCTION_AUTOLINK_RADIUS.
// Outdoor floors (GARDEN_FLOOR_*) use gimmick_cobblestones; indoor
// carpets use gimmick_furniture_carpet_ (s/m/l variants).
static bool ProductionIsFloorModule(const char* module) {
    if (!module) return false;
    return strcmp(module, "gimmick_cobblestones") == 0
        || strcmp(module, "gimmick_furniture_carpet_") == 0
        || strcmp(module, "gimmick_passable_grass") == 0;
}

// Check whether a module name represents a chest-like container.
// Only standard gimmick_chest is treated as a pure container.
// Pickle jars (gimmick_storage_jar / Lv2) are machines, not chests.
static bool ProductionIsChestModule(const char* module) {
    if (!module) return false;
    return strcmp(module, "gimmick_chest") == 0;
}

static bool ProductionReadObject(void* status,
                                 ProductionObjectSnapshot* output,
                                 bool deepNameValidation = true,
                                 bool readOutputMaps = true,
                                 const ProductionStableId* preReadIdentity = nullptr,
                                 u64 preReadGimmickId = 0,
                                 const char* preReadModule = nullptr,
                                 bool skipRevalidation = false) {
    if (!status || !output) return false;
    ProductionObjectSnapshot value = {};
    if (preReadIdentity) {
        value.id = *preReadIdentity;
    } else {
        if (!ProductionReadGimmickIdentity(status, &value.id)) return false;
    }
    // Fast-path: if the caller already read the gimmickId, use it to
    // decide whether the full module string read is worthwhile.
    if (preReadGimmickId) {
        value.gimmickId = preReadGimmickId;
        if (preReadModule) {
            // Full module string was also pre-read.
            strncpy_s(value.module, sizeof(value.module), preReadModule,
                      _TRUNCATE);
        } else {
            // Only the ID was pre-read.  For non-machine IDs we still
            // need the module to check chest/floor.  But first check
            // if the ID alone is a machine — if not, the module read
            // is needed; if it is, the full type check still requires
            // the module string.
            if (!ProductionReadGimmickModule(status, value.module,
                                             sizeof(value.module)))
                return false;
        }
    } else {
        if (!ProductionReadGimmickType(status, &value.gimmickId, value.module,
                                       sizeof(value.module))) return false;
    }
    value.chest = ProductionIsChestModule(value.module);
    value.machine = ProductionIsMachineType(value.gimmickId, value.module);
    value.floor = (!value.chest && !value.machine &&
                    ProductionIsFloorModule(value.module));
    if (!value.chest && !value.machine && !value.floor) return false;

    // Floor tiles only need id + position: skip inventory / process reads.
    if (value.floor) {
        if (NearbyIsReadable(reinterpret_cast<const unsigned char*>(status) +
                           PRODUCTION_POSITION_OFFSET,
                       sizeof(value.position))) {
            memcpy(value.position,
                   reinterpret_cast<const unsigned char*>(status) +
                       PRODUCTION_POSITION_OFFSET,
                       sizeof(value.position));
        }
        // Revalidate identity + type (same guard as chest/machine branch).
        ProductionStableId floorIdAgain = {};
        u64 floorGimmickIdAgain = 0;
        char floorModuleAgain[PRODUCTION_MAX_MODULE_BYTES + 1] = {};
        if (!ProductionReadGimmickIdentity(status, &floorIdAgain) ||
            !ProductionIdEqual(value.id, floorIdAgain) ||
            !ProductionReadGimmickType(status, &floorGimmickIdAgain,
                                      floorModuleAgain, sizeof(floorModuleAgain)) ||
            floorGimmickIdAgain != value.gimmickId ||
            strcmp(floorModuleAgain, value.module) != 0) return false;
        *output = value;
        return true;
    }

    const char* name = *reinterpret_cast<const char* const*>(
        reinterpret_cast<const unsigned char*>(status) +
        PRODUCTION_UNIQUE_NAME_POINTER_OFFSET);
    const size_t nameLength = *reinterpret_cast<const size_t*>(
        reinterpret_cast<const unsigned char*>(status) +
        PRODUCTION_UNIQUE_NAME_POINTER_OFFSET + sizeof(void*));
    if (name && nameLength > 0 && nameLength <= PRODUCTION_MAX_NAME_BYTES &&
        NearbyIsReadable(name, nameLength + 1) && name[nameLength] == '\0') {
        memcpy(value.name, name, nameLength);
        value.name[nameLength] = '\0';
        // Recheck the CString view before publishing the mod-owned copy.
        const char* nameAgain = *reinterpret_cast<const char* const*>(
            reinterpret_cast<const unsigned char*>(status) +
            PRODUCTION_UNIQUE_NAME_POINTER_OFFSET);
        const size_t lengthAgain = *reinterpret_cast<const size_t*>(
            reinterpret_cast<const unsigned char*>(status) +
            PRODUCTION_UNIQUE_NAME_POINTER_OFFSET + sizeof(void*));
        value.named = nameAgain == name && lengthAgain == nameLength &&
                      (!deepNameValidation ||
                       ProductionNameHasVisibleUtf8(value.name, nameLength));
    } else {
        value.name[0] = '\0';
        value.named = false;
    }
    if (NearbyIsReadable(reinterpret_cast<const unsigned char*>(status) +
                       PRODUCTION_POSITION_OFFSET,
                   sizeof(value.position))) {
        memcpy(value.position,
               reinterpret_cast<const unsigned char*>(status) +
                   PRODUCTION_POSITION_OFFSET,
               sizeof(value.position));
    }

    // The registry reference protects lifetime, not identity reuse or a
    // concurrent CString/type mutation.  Revalidate before publishing values.
    // v1.1.27: skipRevalidation=true for scan loop (registry stable during scan,
    // prefix hash detects changes), saves a full identity+type re-read per object.
    if (!skipRevalidation) {
        ProductionStableId identityAgain = {};
        u64 gimmickIdAgain = 0;
        char moduleAgain[PRODUCTION_MAX_MODULE_BYTES + 1] = {};
        if (!ProductionReadGimmickIdentity(status, &identityAgain) ||
            !ProductionIdEqual(value.id, identityAgain) ||
            !ProductionReadGimmickType(status, &gimmickIdAgain, moduleAgain,
                                       sizeof(moduleAgain)) ||
            gimmickIdAgain != value.gimmickId ||
            strcmp(moduleAgain, value.module) != 0) return false;
    }

    NearbyRawPointerVector items = {};
    if (value.chest && NearbyReadRawInventory(status, &items)) {
        value.inventoryValid = true;
        for (size_t index = 0; index < PRODUCTION_CHEST_SLOTS; ++index) {
            if (items.begin[index]) ++value.occupiedSlots;
        }
    }
    // Storage jars may have a different slot count than standard chests
    // (NearbyReadRawInventory requires exactly 30).  Try a relaxed read
    // that accepts 1..64 slots for non-chest containers.
    if (value.chest && !value.inventoryValid) {
        if (NearbyIsReadable(reinterpret_cast<const unsigned char*>(status) +
                             NEARBY_STATUS_ITEMS_OFFSET,
                             sizeof(NearbyRawPointerVector))) {
            NearbyRawPointerVector raw = *reinterpret_cast<const NearbyRawPointerVector*>(
                reinterpret_cast<uintptr_t>(status) + NEARBY_STATUS_ITEMS_OFFSET);
            const uintptr_t begin = reinterpret_cast<uintptr_t>(raw.begin);
            const uintptr_t end = reinterpret_cast<uintptr_t>(raw.end);
            const uintptr_t capacity = reinterpret_cast<uintptr_t>(raw.capacity);
            if (begin && end >= begin && capacity >= end &&
                (end - begin) % sizeof(void*) == 0 &&
                (capacity - begin) % sizeof(void*) == 0) {
                const size_t count = (end - begin) / sizeof(void*);
                const size_t capCount = (capacity - begin) / sizeof(void*);
                if (count >= 1 && count <= 64 && capCount >= count &&
                    NearbyIsReadable(raw.begin, count * sizeof(void*))) {
                    value.inventoryValid = true;
                    const size_t scanCount = count < PRODUCTION_CHEST_SLOTS
                        ? count : PRODUCTION_CHEST_SLOTS;
                    for (size_t index = 0; index < scanCount; ++index) {
                        if (raw.begin[index]) ++value.occupiedSlots;
                    }
                }
            }
        }
    }
    if (value.machine) {
        size_t slotLimit = 0;
        if (!ProductionReadMachineSlotLimit(status, &slotLimit) ||
            slotLimit == 0 || slotLimit > PRODUCTION_MACHINE_SLOTS)
            return false;
        value.slotLimit = slotLimit;
        // A level-one processor commits exactly one timer pair.  Bytes for
        // dormant level-two slots are uncommitted storage and must neither be
        // copied as live timers nor probed through the status maps.
        if (!NearbyIsReadable(
                reinterpret_cast<const unsigned char*>(status) +
                    PRODUCTION_TIME_VALUE_OFFSET,
                slotLimit * sizeof(u64)) ||
            !NearbyIsReadable(
                reinterpret_cast<const unsigned char*>(status) +
                    PRODUCTION_CREATE_TIME_OFFSET,
                slotLimit * sizeof(u64))) return false;
        memcpy(value.start,
               reinterpret_cast<const unsigned char*>(status) +
                   PRODUCTION_TIME_VALUE_OFFSET,
               slotLimit * sizeof(u64));
        memcpy(value.duration,
               reinterpret_cast<const unsigned char*>(status) +
                   PRODUCTION_CREATE_TIME_OFFSET,
               slotLimit * sizeof(u64));
        value.processValid = true;
        if (!readOutputMaps) {
            // Due resolution only needs timers for ready/running/idle
            // classification.  The output maps are read again from the live
            // status inside the transaction preflight, so skipping them here
            // removes the most expensive per-slot map lookups from the
            // every-transaction resolve phase.
        } else for (size_t slot = 0; slot < slotLimit; ++slot) {
            char idKey[8] = {};
            // itemVal0 is eight packed key bytes plus the C-string terminator.
            char countKey[9] = {};
            _snprintf_s(idKey, sizeof(idKey), _TRUNCATE, "itemID%zu", slot);
            _snprintf_s(countKey, sizeof(countKey), _TRUNCATE, "itemVal%zu", slot);
            u64 idValue = 0;
            u64 countValue = 0;
            if (!ProductionReadStatusMapValue(status, 0x58, idKey, &idValue,
                                              false) ||
                !ProductionReadStatusMapValue(status, 0x18, countKey,
                                              &countValue, true)) {
                value.processValid = false;
                break;
            }
            value.outputItemId[slot] = idValue;
            value.outputCount[slot] = static_cast<int>(countValue);
            value.inputItemId[slot] = 0;
            value.inputCount[slot] = 0;
            if (idValue && countValue > 0 && value.duration[slot] > 0) {
                const auto* idx = village_qol::production_automation::
                    ProductionRecipeMachineIndexLookup(value.gimmickId);
                if (idx) {
                    const auto& tbl =
                        village_qol::production_automation::kProcessingRecipeTable;
                    for (u32 ri = 0; ri < idx->recipeCount; ++ri) {
                        const auto& recipe = tbl[idx->recipeIndices[ri]];
                        if (recipe.outputItemId != idValue ||
                            recipe.outputCount != static_cast<unsigned int>(countValue) ||
                            recipe.duration > UINT64_MAX / 60ULL ||
                            recipe.duration * 60ULL != value.duration[slot]) continue;
                        for (size_t ii = 0; ii < 8; ++ii) {
                            if (recipe.inputItemIds[ii] && recipe.inputCounts[ii]) {
                                value.inputItemId[slot] = recipe.inputItemIds[ii];
                                value.inputCount[slot] = recipe.inputCounts[ii];
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    *output = value;
    return true;
}

static bool ProductionReadRegistryHead(void* save, void** sentinelOut,
                                       size_t* declaredOut) {
    if (!save || !sentinelOut || !declaredOut ||
        !NearbyIsReadable(reinterpret_cast<unsigned char*>(save) +
                              PRODUCTION_GIMMICK_LIST_OFFSET,
                          sizeof(void*) + sizeof(u64))) return false;
    void* sentinel = *reinterpret_cast<void**>(
        reinterpret_cast<unsigned char*>(save) +
        PRODUCTION_GIMMICK_LIST_OFFSET);
    const u64 declared = *reinterpret_cast<const u64*>(
        reinterpret_cast<unsigned char*>(save) +
        PRODUCTION_GIMMICK_LIST_OFFSET + sizeof(void*));
    if (!sentinel || declared > PRODUCTION_MAX_GIMMICKS ||
        !NearbyIsReadable(sentinel, sizeof(void*) * 2)) return false;
    *sentinelOut = sentinel;
    *declaredOut = static_cast<size_t>(declared);
    return true;
}

static ProductionBinding* ProductionFindBindingLocked(
        const ProductionStableId& machine, bool create) {
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        if (ProductionIdEqual(g_productionBindings[index].machine, machine))
            return &g_productionBindings[index];
    }
    if (!create || g_productionBindingCount >= PRODUCTION_MAX_BINDINGS)
        return nullptr;
    ProductionBinding& binding =
        g_productionBindings[g_productionBindingCount++];
    binding = {};
    binding.machine = machine;
    binding.enabled = false;
    binding.committed = false;
    binding.autoLinked = false;
    binding.state = ProductionState::Unbound;
    binding.reason = ProductionReason::InputUnbound;
    binding.groupIndex = SIZE_MAX;
    binding.inputRotation = 0;
    binding.outputRotation = 0;
    binding.inputTriedCount = 0;
    binding.outputTriedCount = 0;
    return &binding;
}

static const ProductionObjectSnapshot* ProductionFindObject(
        const ProductionObjectSnapshot* values, size_t count,
        const ProductionStableId& id, size_t* matchesOut = nullptr) {
    const ProductionObjectSnapshot* found = nullptr;
    size_t matches = 0;
    // Fast path: use the registry index cache to avoid a linear scan.
    // The cache stores ordinal positions from the last full registry walk.
    // This path also serves matchesOut callers: when the cache confirms a
    // unique entry, matches is exactly 1 and we can short-circuit the scan.
    if (count > 0 && values == g_productionScanObjects) {
        size_t cachedIndex = SIZE_MAX;
        bool cachedUnique = false;
        const u64 loadGen =
            g_autoPetLoadCompletionGeneration.load(std::memory_order_acquire);
        if (ProductionRegistryIndexCacheLookup(
                id, g_productionRegistryIndexCacheDeclared, loadGen,
                &cachedIndex, &cachedUnique) &&
            cachedUnique && cachedIndex < count &&
            ProductionIdEqual(values[cachedIndex].id, id)) {
            if (matchesOut) *matchesOut = 1;
            return &values[cachedIndex];
        }
    }
    // Fallback: linear scan.
    for (size_t index = 0; index < count; ++index) {
        if (!ProductionIdEqual(values[index].id, id)) continue;
        found = &values[index];
        ++matches;
    }
    if (matchesOut) *matchesOut = matches;
    return matches == 1 ? found : nullptr;
}

// Find an object by ID, but skip a specific index.  Used by the chest
// rotation functions to avoid colliding input and output onto the same
// chest when the group has >= 3 chests.
static const ProductionObjectSnapshot* ProductionFindObjectExcluding(
        const ProductionObjectSnapshot* values, size_t count,
        const ProductionStableId& id, const ProductionStableId& excludeId) {
    // If id == excludeId, there is no valid result.
    if (ProductionIdEqual(id, excludeId)) return nullptr;
    for (size_t index = 0; index < count; ++index) {
        if (!ProductionIdEqual(values[index].id, id)) continue;
        return &values[index];
    }
    return nullptr;
}

static void ProductionCopyLogToken(const char* input, char* output,
                                   size_t outputCount) {
    if (!output || outputCount == 0) return;
    size_t used = 0;
    for (; input && input[used] && used + 1 < outputCount; ++used) {
        const unsigned char value = static_cast<unsigned char>(input[used]);
        output[used] = (value <= 0x20 || value == '"' || value == '\\')
                           ? '_' : static_cast<char>(value);
    }
    output[used] = '\0';
}

static void ProductionResolveLogName(
        const ProductionObjectSnapshot* objects, size_t objectCount,
        const ProductionStableId& id, bool bound, char* output,
        size_t outputCount) {
    if (!output || outputCount == 0) return;
    if (!bound) {
        strcpy_s(output, outputCount, "unbound");
        return;
    }
    const ProductionObjectSnapshot* object =
        ProductionFindObject(objects, objectCount, id);
    if (!object || !object->named) {
        strcpy_s(output, outputCount, "unresolved");
        return;
    }
    ProductionCopyLogToken(object->name, output, outputCount);
}

static u64 ProductionInferRecipeId(
        const ProductionObjectSnapshot* machine, size_t slot) {
    if (!machine || !machine->machine || slot >= PRODUCTION_MACHINE_SLOTS ||
        !machine->outputItemId[slot] || machine->outputCount[slot] <= 0 ||
        !machine->duration[slot]) return 0;
    u64 found = 0;
    const auto* idx = village_qol::production_automation::
        ProductionRecipeMachineIndexLookup(machine->gimmickId);
    if (idx) {
        const auto& tbl =
            village_qol::production_automation::kProcessingRecipeTable;
        for (u32 ri = 0; ri < idx->recipeCount; ++ri) {
            const auto& recipe = tbl[idx->recipeIndices[ri]];
            if (recipe.outputItemId != machine->outputItemId[slot] ||
                recipe.outputCount !=
                    static_cast<std::uint32_t>(machine->outputCount[slot]) ||
                recipe.duration > UINT64_MAX / 60ULL ||
                recipe.duration * 60ULL != machine->duration[slot]) continue;
            if (found && found != recipe.recipeId) return 0;
            found = recipe.recipeId;
        }
    }
    return found;
}

static void ProductionEvaluateBinding(
        ProductionBinding* binding, const ProductionObjectSnapshot* objects,
        size_t objectCount, std::int64_t gameSecond) {
    if (!binding) return;
    size_t machineMatches = 0;
    const ProductionObjectSnapshot* machine = ProductionFindObject(
        objects, objectCount, binding->machine, &machineMatches);
    if (!machine) {
        binding->state = ProductionState::Faulted;
        binding->reason = machineMatches > 1 ? ProductionReason::AmbiguousIdentity
                                            : ProductionReason::MachineIdentityMissing;
        return;
    }
    if (!machine->machine) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::InvalidObjectType;
        return;
    }
    if (!machine->processValid || machine->slotLimit == 0 ||
        machine->slotLimit > PRODUCTION_MACHINE_SLOTS) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::RegistryInvalid;
        return;
    }
    // When paused, still drain finished output so produced items
    // are not permanently lost.  Only OutputReady is allowed; no new
    // input is started and running machines stay running.
    if (!binding->enabled) {
        if (binding->hasOutput) {
            size_t outMatches = 0;
            const ProductionObjectSnapshot* outObj = ProductionFindObject(
                objects, objectCount, binding->output, &outMatches);
            if (outObj && outObj->chest && outObj->inventoryValid &&
                (outObj->named || binding->autoLinked)) {
                bool drainReady = false;
                for (size_t slot = 0; slot < machine->slotLimit; ++slot) {
                    if (!machine->start[slot] || !machine->duration[slot])
                        continue;
                    if (gameSecond >= 0 &&
                        machine->start[slot] <= INT64_MAX &&
                        machine->duration[slot] <= INT64_MAX &&
                        gameSecond >=
                            static_cast<std::int64_t>(machine->start[slot]) &&
                        gameSecond -
                            static_cast<std::int64_t>(machine->start[slot]) >=
                            static_cast<std::int64_t>(
                                machine->duration[slot])) {
                        drainReady = true;
                        break;
                    }
                }
                if (drainReady) {
                    binding->state = ProductionState::OutputReady;
                    binding->reason = ProductionReason::OutputNeedsTransfer;
                    return;
                }
                // Passive machines: no timer-based drain detection above.
                // Allow drain if output slots have items without timers.
                if (ProductionIsPassiveMachine(machine->gimmickId)) {
                    for (size_t slot = 0; slot < machine->slotLimit; ++slot) {
                        if (machine->outputItemId[slot] &&
                            machine->outputCount[slot] > 0) {
                            binding->state = ProductionState::OutputReady;
                            binding->reason =
                                ProductionReason::OutputNeedsTransfer;
                            return;
                        }
                    }
                }
            }
        }
        binding->state = ProductionState::Idle;
        binding->reason = ProductionReason::PausedByUser;
        return;
    }
    if (!binding->hasOutput) {
        binding->state = ProductionState::Unbound;
        binding->reason = ProductionReason::OutputUnbound;
        return;
    }
    if (binding->hasInput &&
        ProductionIdEqual(binding->input, binding->output) &&
        !binding->autoLinked) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::SameChestRejected;
        return;
    }
    size_t outputMatches = 0;
    const ProductionObjectSnapshot* output = ProductionFindObject(
        objects, objectCount, binding->output, &outputMatches);
    if (!output) {
        binding->state = ProductionState::Faulted;
        binding->reason = outputMatches > 1
            ? ProductionReason::AmbiguousIdentity
            : ProductionReason::ChestIdentityMissing;
        return;
    }
    if (!output->chest) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::InvalidObjectType;
        return;
    }
    if (!output->inventoryValid) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::RegistryInvalid;
        return;
    }
    if (!output->named && !binding->autoLinked) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::ChestNameInvalid;
        return;
    }
    bool running = false;
    bool ready = false;
    for (size_t slot = 0; slot < machine->slotLimit; ++slot) {
        if (!machine->start[slot] || !machine->duration[slot])
            continue;
        const bool finished =
            gameSecond >= 0 && machine->start[slot] <= INT64_MAX &&
            machine->duration[slot] <= INT64_MAX &&
            gameSecond >= static_cast<std::int64_t>(machine->start[slot]) &&
            gameSecond - static_cast<std::int64_t>(machine->start[slot]) >=
                static_cast<std::int64_t>(machine->duration[slot]);
        if (finished) ready = true;
        else running = true;
    }
    if (ready) {
        binding->state = ProductionState::OutputReady;
        binding->reason = ProductionReason::OutputNeedsTransfer;
        return;
    }
    if (running) {
        binding->state = ProductionState::Running;
        binding->reason = ProductionReason::RunningNativeTimer;
        return;
    }
    // Passive machines (beehive/sap extractor/sericulture box) may not use
    // standard timers.  If all timer slots were skipped, check output slots
    // directly for producible items.  Only a slot with a real output may
    // classify as OutputReady; an idle passive machine must fall through to
    // the chest-input validation below so it can be fed (WaitingInput).
    if (!ready && !running &&
        ProductionIsPassiveMachine(machine->gimmickId)) {
        for (size_t slot = 0; slot < machine->slotLimit; ++slot) {
            if (machine->outputItemId[slot] &&
                machine->outputCount[slot] > 0) {
                binding->state = ProductionState::OutputReady;
                binding->reason = ProductionReason::OutputNeedsTransfer;
                return;
            }
        }
    }

    // Input identity must never block delivery of an already-finished product.
    // Validate it only after output-ready/running classification; an idle,
    // fully bound machine can then enter the separate chest-input transaction.
    if (!binding->hasInput) {
        binding->state = ProductionState::Unbound;
        binding->reason = ProductionReason::InputUnbound;
        return;
    }
    size_t inputMatches = 0;
    const ProductionObjectSnapshot* input = ProductionFindObject(
        objects, objectCount, binding->input, &inputMatches);
    if (!input) {
        binding->state = ProductionState::Faulted;
        binding->reason = inputMatches > 1
            ? ProductionReason::AmbiguousIdentity
            : ProductionReason::ChestIdentityMissing;
        return;
    }
    if (!input->chest) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::InvalidObjectType;
        return;
    }
    if (!input->inventoryValid) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::RegistryInvalid;
        return;
    }
    if (!input->named && !binding->autoLinked) {
        binding->state = ProductionState::Faulted;
        binding->reason = ProductionReason::ChestNameInvalid;
        return;
    }
    binding->state = ProductionState::WaitingInput;
    binding->reason = ProductionReason::WaitingIngredients;
}

static void ProductionPublish(const ProductionObjectSnapshot* objects,
                              size_t objectCount, std::int64_t gameSecond) {
    ProductionPublishedState next = {};
    next.generation = g_productionSequence.load(std::memory_order_relaxed);
    next.gameSecond = gameSecond;
    next.activeMachine = ProductionGetActiveMachine();
    next.activeMachinePresent = next.activeMachine.mapId != 0 &&
                                next.activeMachine.uniqueId != 0;
    for (size_t index = 0; index < objectCount &&
                           next.chestCount < PRODUCTION_MAX_CHESTS; ++index) {
        if (objects[index].chest && objects[index].named &&
            objects[index].inventoryValid)
            next.chests[next.chestCount++] = objects[index];
    }

    AcquireSRWLockShared(&g_productionBindingLock);
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        const ProductionBinding& binding = g_productionBindings[index];
        if (next.activeMachinePresent &&
            ProductionIdEqual(binding.machine, next.activeMachine)) {
            next.activeState = binding.state;
            next.activeReason = binding.reason;
            break;
        }
    }
    ReleaseSRWLockShared(&g_productionBindingLock);
    AcquireSRWLockExclusive(&g_productionPublishLock);
    g_productionPublished = next;
    ReleaseSRWLockExclusive(&g_productionPublishLock);
}


// ---- verified output transfer (machine -> output chest) ------------------
// The native GimmickProcessPopItem path throws the finished item into the
// world for player pickup.  Its inner completion helper instead clears the
// machine slot (+0x288/+0x2a0) and the machine item list (+0x2b8) and hands
// the produced CItemStatus to the caller with one owned reference.  The mod
// therefore delivers machine outputs directly into the verified output chest
// using the same count/refcount primitives the nearby sort already proves.
// Every step is readback-verified; any mismatch latches a transfer fault,
// quarantines the item (never released), and stops further writes.
using ProductionOutputHelperFunction = void* (__fastcall *)(void**, void*, int);
using ProductionItemAllocateFunction = void* (__fastcall *)(size_t);
using ProductionItemCtorFunction = void* (__fastcall *)(void*, void*, u64);
using ProductionItemSetCountFunction = void (__fastcall *)(void*, int);
using ProductionItemIsStackableFunction = bool (__fastcall *)(void*);
// native_load: game's original item-loading function (RVA 0x26FF20).
// Windows x64 ABI: (rcx=machine/ctx, rdx=item/param, r8, r9).
// Exact parameter semantics under investigation — to be confirmed via
// further disassembly before the transfer-logic rewrite.
using ProductionNativeLoadFunction = void* (__fastcall *)(void*, void*, void*, void*);
// native_collect: game's original item-collection function (RVA 0x270C10).
// Windows x64 ABI: (rcx=machine/ctx, rdx=item/param, r8, r9).
using ProductionNativeCollectFunction = void* (__fastcall *)(void*, void*, void*, void*);
static ProductionOutputHelperFunction g_productionOutputHelper = nullptr;
static ProductionItemAllocateFunction g_productionItemAllocate = nullptr;
static ProductionItemCtorFunction g_productionItemCtor = nullptr;
static ProductionItemSetCountFunction g_productionItemSetCount = nullptr;
static ProductionItemIsStackableFunction g_productionItemIsStackable = nullptr;
static ProductionNativeLoadFunction g_productionNativeLoad = nullptr;
static ProductionNativeCollectFunction g_productionNativeCollect = nullptr;
struct ProductionMapLookupResult {
    void* node;
    unsigned char inserted;
    unsigned char padding[7];
};
using ProductionStatusMapLookupFunction = ProductionMapLookupResult* (
    __fastcall *)(void*, ProductionMapLookupResult*, const u64*);
using ProductionStatusEventNotifyFunction = void (__fastcall *)(void*);
using ProductionStatusEventOwnerFunction = void* (__fastcall *)(void*);
static ProductionStatusMapLookupFunction g_productionStatusIntLookup = nullptr;
static ProductionStatusMapLookupFunction g_productionStatusU64Lookup = nullptr;
static ProductionStatusMapLookupFunction g_productionStatusEventLookup = nullptr;
static ProductionStatusEventNotifyFunction g_productionStatusEventNotify =
    nullptr;
static std::atomic<void*> g_productionTransferQuarantine{nullptr};
static std::atomic<bool> g_productionTransferFaulted{false};
// A failed multi-source rollback may need to retain more than one intrusive
// guard.  The process is faulted immediately, so this bounded list is append-
// only and intentionally released only by process teardown.
static void* g_productionInputQuarantine[
    PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT] = {};
static size_t g_productionInputQuarantineCount = 0;
// `gameSecond` and recipe durations use the same native 60-tick unit.  Missing
// ingredients and full output chests therefore sleep with the game clock: a
// paused game performs no periodic registry work at all.
static constexpr std::int64_t PRODUCTION_INPUT_RETRY_GAME_TICKS = 60 * 30;
static constexpr std::int64_t PRODUCTION_OUTPUT_RETRY_GAME_TICKS = 60 * 60;
static constexpr std::int64_t PRODUCTION_RESOLVE_RETRY_GAME_TICKS = 60 * 60;
// A mature slot that could not be proven finished, or a machine status whose
// world object is temporarily culled, re-checks on the next game-minute pair
// instead of waiting a full hour.  Exponential backoff still applies, so a
// genuinely wedged binding cannot turn into a per-frame retry storm.
static constexpr std::int64_t PRODUCTION_TRANSIENT_OUTPUT_RETRY_GAME_TICKS =
    60 * 2;
static constexpr std::int64_t PRODUCTION_TRANSIENT_INPUT_RETRY_GAME_TICKS =
    60 * 2;
static constexpr std::int64_t PRODUCTION_MAX_RETRY_GAME_TICKS = 60 * 1800;
static size_t g_productionOutputDueCursor = 0;
static size_t g_productionInputDueCursor = 0;
static u64 g_productionScheduleRevisionObserved = 0;
static u64 g_productionScheduleLoadInFlightObserved = 0;
static u64 g_productionScheduleLoadGenerationObserved = 0;
static u64 g_productionScheduleLoadReadyObserved = 0;
static u64 g_productionScheduleLoadHandledObserved = 0;
static u64 g_productionScheduleWorldEpochObserved = 0;
static u64 g_productionScheduleClockWakeObserved = 0;
// The verified main-world CGameTime hook publishes scalar snapshots.  The
// scheduler retains only these value epochs; every save/status/inventory
// pointer is resolved after a due candidate is selected and dies with that
// callback.
static std::int64_t g_productionScheduleGameSecondObserved = 0;
static u64 g_productionScheduleGameSecondWorldEpoch = 0;
static u64 g_productionScheduleGameSecondLoadGeneration = 0;
static bool g_productionScheduleGameSecondValid = false;

struct ProductionStatusHit {
    ProductionStableId id;
    void* status;
};

// ---- Input chest fingerprint cache -------------------------------------------
// Populated once per scan: every chest status pointer seen during the registry
// walk is recorded so the binding evaluation phase can read live input chest
// content and detect that the player added ingredients.  This lets a
// WaitingInput binding wake early instead of waiting out the exponential
// input backoff.  Written and read on the main thread only, inside one scan
// callback, so no additional lock is required.
static ProductionStatusHit g_productionScanChestStatus[
    PRODUCTION_MAX_CHESTS] = {};
static size_t g_productionScanChestStatusCount = 0;

static const ProductionStatusHit* ProductionFindScanChestStatus(
        const ProductionStableId& id) {
    for (size_t index = 0; index < g_productionScanChestStatusCount; ++index) {
        if (ProductionIdEqual(g_productionScanChestStatus[index].id, id))
            return &g_productionScanChestStatus[index];
    }
    return nullptr;
}

// FNV-1a 64 hash of a chest's item slots (itemId/count/rank).  Slots are
// hashed in position order, so moving items between slots changes the hash;
// this is fine for the wake purpose (any change re-triggers evaluation).
static uint64_t ProductionChestFingerprint(void* chestStatus) {
    if (!chestStatus) return 0;
    NearbyRawPointerVector inventory = {};
    if (!NearbyReadRawInventoryRelaxed(chestStatus, &inventory)) return 0;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(inventory.begin);
    const uintptr_t end = reinterpret_cast<uintptr_t>(inventory.end);
    if (!begin || end < begin || (end - begin) % sizeof(void*) != 0) return 0;
    const size_t effectiveSlots = (end - begin) / sizeof(void*);
    const size_t slotCount = effectiveSlots <= PRODUCTION_CHEST_SLOTS
        ? effectiveSlots : PRODUCTION_CHEST_SLOTS;
    uint64_t hash = 1469598103934665603ULL;  // FNV-1a offset basis
    for (size_t slot = 0; slot < slotCount; ++slot) {
        void* item = inventory.begin[slot];
        if (!item) continue;
        NearbyItemInfo info = {};
        if (!NearbyReadItem(item, &info)) continue;
        hash ^= info.itemId;
        hash *= 1099511628211ULL;
        hash ^= static_cast<uint64_t>(info.rank);
        hash *= 1099511628211ULL;
        hash ^= static_cast<uint64_t>(info.stackCount);
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool ProductionReadStatusObjectId(void* status, u64* objectId);

// One native world-object -> component -> status round trip.  Unlike earlier
// diagnostics this helper deliberately accepts a world registry whose native
// resolver refreshes the registry pointer between calls: the resolved world
// object itself is revalidated by object id, and the component/status pair is
// proven against it.  Returns false when the round trip is merely unavailable
// (culled/remote object), which callers treat as a soft signal, not a fault.
static bool ProductionComponentRoundTripsStatus(void* status, u64 objectId) {
    if (!status || !objectId || !g_nearbyResolveWorldObject ||
        !g_productionResolveComponent) return false;
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    void** worldRegistry = reinterpret_cast<void**>(
        base + RVA_NEARBY_WORLD_OBJECT_REGISTRY);
    if (!NearbyIsReadable(worldRegistry, sizeof(void*)) || !*worldRegistry)
        return false;
    void* worldObject = g_nearbyResolveWorldObject(*worldRegistry, objectId);
    if (!worldObject ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(worldObject) +
                PRODUCTION_WORLD_OBJECT_ID_OFFSET,
            sizeof(u64)) ||
        *reinterpret_cast<const u64*>(
            reinterpret_cast<unsigned char*>(worldObject) +
                PRODUCTION_WORLD_OBJECT_ID_OFFSET) != objectId)
        return false;
    void* component = g_productionResolveComponent(worldObject);
    if (!component ||
        !NearbyIsReadable(component,
                          PRODUCTION_COMPONENT_STATUS_OFFSET + sizeof(void*)) ||
        *reinterpret_cast<const uintptr_t*>(component) !=
            base + RVA_PRODUCTION_COMPONENT_VTABLE ||
        *reinterpret_cast<void**>(
            reinterpret_cast<unsigned char*>(component) +
                PRODUCTION_COMPONENT_STATUS_OFFSET) != status)
        return false;
    return true;
}

static bool ProductionStatusHasLiveComponent(void* status) {
    if (!status || !g_nearbyResolveWorldObject ||
        !g_productionResolveComponent) return false;

    // +0x338 belongs to CCom_Gimmick and points *to* CGimmickStatus.  It is
    // not a reverse component pointer inside CGimmickStatus.  Reading
    // status+0x338 happened to yield a readable value for the observed
    // beehives, but rejected the other verified processing-machine layouts
    // before their recipe planner was reached.  Resolve the owning world
    // object from the status' stable object link and prove the real native
    // world-object -> component -> status round trip instead.
    ProductionStableId identityBefore = {};
    u64 gimmickIdBefore = 0;
    char moduleBefore[PRODUCTION_MAX_MODULE_BYTES + 1] = {};
    u64 objectId = 0;
    if (!ProductionReadGimmickIdentity(status, &identityBefore) ||
        !ProductionReadGimmickType(status, &gimmickIdBefore, moduleBefore,
                                   sizeof(moduleBefore)) ||
        !ProductionIsMachineType(gimmickIdBefore, moduleBefore) ||
        !ProductionReadStatusObjectId(status, &objectId) ||
        !ProductionComponentRoundTripsStatus(status, objectId)) return false;

    ProductionStableId identityAfter = {};
    u64 gimmickIdAfter = 0;
    char moduleAfter[PRODUCTION_MAX_MODULE_BYTES + 1] = {};
    return ProductionReadGimmickIdentity(status, &identityAfter) &&
           ProductionIdEqual(identityBefore, identityAfter) &&
           ProductionReadGimmickType(status, &gimmickIdAfter, moduleAfter,
                                     sizeof(moduleAfter)) &&
           gimmickIdAfter == gimmickIdBefore &&
           strcmp(moduleAfter, moduleBefore) == 0;
}

// The native transaction write path may target a machine whose world object
// is currently culled (remote field objects advance their timers while their
// world object is not resident).  Requiring the full component round trip
// therefore stalled every remote Lv2 machine and even nearby machines when the
// resolver refreshed the registry snapshot.  The actual native helpers operate
// on the status pointer found in the save's live gimmick list, so the write
// gate is: fresh identity + machine type + status object link from the live
// status, plus a best-effort component round trip recorded as telemetry.
struct ProductionMachineStatusValidation {
    bool ready;
    bool componentRoundTrip;
    bool objectLinkReadable;
};

static bool ProductionValidateMachineStatusForNativeTransaction(
        void* status, const ProductionStableId* expected,
        ProductionMachineStatusValidation* validationOut = nullptr) {
    ProductionMachineStatusValidation validation = {};
    if (!status) {
        if (validationOut) *validationOut = validation;
        return false;
    }
    ProductionStableId identity = {};
    u64 gimmickId = 0;
    char module[PRODUCTION_MAX_MODULE_BYTES + 1] = {};
    if (!ProductionReadGimmickIdentity(status, &identity) ||
        (expected && !ProductionIdEqual(identity, *expected)) ||
        !ProductionReadGimmickType(status, &gimmickId, module,
                                   sizeof(module)) ||
        !ProductionIsMachineType(gimmickId, module)) {
        if (validationOut) *validationOut = validation;
        return false;
    }
    u64 objectId = 0;
    validation.objectLinkReadable =
        ProductionReadStatusObjectId(status, &objectId);
    validation.componentRoundTrip =
        validation.objectLinkReadable &&
        ProductionStatusHasLiveComponent(status);
    validation.ready = true;
    if (validationOut) *validationOut = validation;
    return true;
}

static bool ProductionGetStatusEventOwner(
        void* status, void** eventOwnerOut) {
    if (!status || !eventOwnerOut) return false;
    unsigned char* interfaceObject =
        reinterpret_cast<unsigned char*>(status) + 0x10;
    if (!NearbyIsReadable(interfaceObject, sizeof(void*)) ||
        !NearbyIsReadable(*reinterpret_cast<void**>(interfaceObject),
                          sizeof(void*) * 2)) return false;
    void** vtable = *reinterpret_cast<void***>(interfaceObject);
    ProductionStatusEventOwnerFunction getter =
        reinterpret_cast<ProductionStatusEventOwnerFunction>(vtable[1]);
    if (!getter || !NearbyIsReadable(reinterpret_cast<void*>(getter), 1))
        return false;
    void* eventOwner = getter(interfaceObject);
    if (!eventOwner ||
        !NearbyIsReadable(reinterpret_cast<unsigned char*>(eventOwner) + 0x08,
                          0x38)) return false;
    *eventOwnerOut = eventOwner;
    return true;
}

static bool ProductionCanSetStatusInt(void* status) {
    void* eventOwner = nullptr;
    return status && g_productionStatusIntLookup &&
           g_productionStatusEventLookup && g_productionStatusEventNotify &&
           NearbyIsReadable(reinterpret_cast<unsigned char*>(status) + 0x18,
                            0x38) &&
           ProductionGetStatusEventOwner(status, &eventOwner);
}

static bool ProductionCanSetStatusU64(void* status) {
    void* eventOwner = nullptr;
    return status && g_productionStatusU64Lookup &&
           g_productionStatusEventLookup && g_productionStatusEventNotify &&
           NearbyIsReadable(reinterpret_cast<unsigned char*>(status) + 0x58,
                            0x38) &&
           ProductionGetStatusEventOwner(status, &eventOwner) &&
           NearbyIsReadable(
               reinterpret_cast<unsigned char*>(eventOwner) + 0x48, 0x38);
}

// Native-equivalent body of MapObjectSetStatusValue for the integer status
// map.  This deliberately bypasses only the Lua/TLS wrapper; node creation and
// the registered status-change observers use the exact native functions used
// by the wrapper and by the output helper itself.
static bool ProductionSetStatusInt(void* status, const char* key, int value) {
    if (!status || !key || strlen(key) > sizeof(u64) ||
        !ProductionCanSetStatusInt(status)) return false;
    u64 packedKey = 0;
    memcpy(&packedKey, key, strlen(key));
    ProductionMapLookupResult statusResult = {};
    ProductionMapLookupResult* statusFound = g_productionStatusIntLookup(
        reinterpret_cast<unsigned char*>(status) + 0x18, &statusResult,
        &packedKey);
    if (!statusFound || !statusFound->node ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(statusFound->node) + 0x18,
            sizeof(int))) return false;
    int* nativeValue = reinterpret_cast<int*>(
        reinterpret_cast<unsigned char*>(statusFound->node) + 0x18);
    if (*nativeValue == value) return true;
    *nativeValue = value;

    void* eventOwner = nullptr;
    if (!ProductionGetStatusEventOwner(status, &eventOwner)) return false;
    ProductionMapLookupResult eventResult = {};
    ProductionMapLookupResult* eventFound = g_productionStatusEventLookup(
        reinterpret_cast<unsigned char*>(eventOwner) + 0x08, &eventResult,
        &packedKey);
    if (!eventFound || !eventFound->node ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(eventFound->node) + 0x18,
            0x20)) return false;
    g_productionStatusEventNotify(
        reinterpret_cast<unsigned char*>(eventFound->node) + 0x18);
    u64 readback = 0;
    return ProductionReadStatusMapValue(status, 0x18, key, &readback, true) &&
           static_cast<int>(readback) == value;
}

// Native-equivalent QWORD status publication used for itemID{slot}.  The
// native lookup is find-or-insert; rolling a failed transaction back to zero
// may therefore retain a zero-valued node, which is semantically identical to
// a getter miss.  Never unlink a node or clear the whole status map.
static bool ProductionSetStatusU64(void* status, const char* key, u64 value) {
    if (!status || !key || strlen(key) > sizeof(u64) ||
        !ProductionCanSetStatusU64(status)) return false;
    u64 packedKey = 0;
    memcpy(&packedKey, key, strlen(key));
    ProductionMapLookupResult statusResult = {};
    ProductionMapLookupResult* statusFound = g_productionStatusU64Lookup(
        reinterpret_cast<unsigned char*>(status) + 0x58, &statusResult,
        &packedKey);
    if (!statusFound || !statusFound->node ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(statusFound->node) + 0x18,
            sizeof(u64))) return false;
    u64* nativeValue = reinterpret_cast<u64*>(
        reinterpret_cast<unsigned char*>(statusFound->node) + 0x18);
    if (*nativeValue == value) return true;
    *nativeValue = value;

    void* eventOwner = nullptr;
    if (!ProductionGetStatusEventOwner(status, &eventOwner) ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(eventOwner) + 0x48, 0x38))
        return false;
    ProductionMapLookupResult eventResult = {};
    ProductionMapLookupResult* eventFound = g_productionStatusEventLookup(
        reinterpret_cast<unsigned char*>(eventOwner) + 0x48, &eventResult,
        &packedKey);
    if (!eventFound || !eventFound->node ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(eventFound->node) + 0x18,
            0x20)) return false;
    g_productionStatusEventNotify(
        reinterpret_cast<unsigned char*>(eventFound->node) + 0x18);
    u64 readback = 0;
    return ProductionReadStatusMapValue(status, 0x58, key, &readback,
                                        false) &&
           readback == value;
}

// Remote/culled machine statuses still live in the save's gimmick registry and
// their maps remain valid, but their world-event owner can be temporarily
// unreachable.  The native map write path for such a status must then skip the
// observer notification rather than fail the whole transaction: the value is
// still written through the exact native find-or-insert lookup, and every
// consumer reads the map directly.  UI observers refresh when the machine
// loads again, which is the same moment the native Lua wrapper could notify.
static bool ProductionCanSetStatusIntSilent(void* status) {
    return status && g_productionStatusIntLookup &&
           NearbyIsReadable(
               reinterpret_cast<unsigned char*>(status) + 0x18, 0x38);
}

static bool ProductionCanSetStatusU64Silent(void* status) {
    return status && g_productionStatusU64Lookup &&
           NearbyIsReadable(
               reinterpret_cast<unsigned char*>(status) + 0x58, 0x38);
}

static bool ProductionSetStatusIntSilent(void* status, const char* key,
                                         int value) {
    if (!status || !key || strlen(key) > sizeof(u64) ||
        !ProductionCanSetStatusIntSilent(status)) return false;
    u64 packedKey = 0;
    memcpy(&packedKey, key, strlen(key));
    ProductionMapLookupResult statusResult = {};
    ProductionMapLookupResult* statusFound = g_productionStatusIntLookup(
        reinterpret_cast<unsigned char*>(status) + 0x18, &statusResult,
        &packedKey);
    if (!statusFound || !statusFound->node ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(statusFound->node) + 0x18,
            sizeof(int))) return false;
    int* nativeValue = reinterpret_cast<int*>(
        reinterpret_cast<unsigned char*>(statusFound->node) + 0x18);
    *nativeValue = value;
    u64 readback = 0;
    return ProductionReadStatusMapValue(status, 0x18, key, &readback, true) &&
           static_cast<int>(readback) == value;
}

static bool ProductionSetStatusU64Silent(void* status, const char* key,
                                         u64 value) {
    if (!status || !key || strlen(key) > sizeof(u64) ||
        !ProductionCanSetStatusU64Silent(status)) return false;
    u64 packedKey = 0;
    memcpy(&packedKey, key, strlen(key));
    ProductionMapLookupResult statusResult = {};
    ProductionMapLookupResult* statusFound = g_productionStatusU64Lookup(
        reinterpret_cast<unsigned char*>(status) + 0x58, &statusResult,
        &packedKey);
    if (!statusFound || !statusFound->node ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(statusFound->node) + 0x18,
            sizeof(u64))) return false;
    u64* nativeValue = reinterpret_cast<u64*>(
        reinterpret_cast<unsigned char*>(statusFound->node) + 0x18);
    *nativeValue = value;
    u64 readback = 0;
    return ProductionReadStatusMapValue(status, 0x58, key, &readback,
                                        false) &&
           readback == value;
}

static bool ProductionReadMachineSlotLimit(void* machineStatus,
                                           size_t* limitOut,
                                           void** dataOut) {
    if (!machineStatus || !limitOut) return false;
    void* holder = nullptr;
    void* data = nullptr;
    if (!ProductionReadPointer(machineStatus, PRODUCTION_GIMMICK_DATA_OFFSET,
                               &holder) ||
        !ProductionReadPointer(holder, 0, &data) ||
        !NearbyIsReadable(reinterpret_cast<unsigned char*>(data) + 0x128,
                          sizeof(void*) + sizeof(u64))) return false;
    const char* level = *reinterpret_cast<const char* const*>(
        reinterpret_cast<unsigned char*>(data) + 0x128);
    const u64 levelLength = *reinterpret_cast<const u64*>(
        reinterpret_cast<unsigned char*>(data) + 0x130);
    const bool levelTwo = levelLength == 1 && level &&
        NearbyIsReadable(level, 1) && level[0] == '2';
    *limitOut = levelTwo ? PRODUCTION_MACHINE_SLOTS : 1;
    if (dataOut) *dataOut = data;
    return true;
}

static bool ProductionReadMachineItemBacking(void* machineStatus,
                                             size_t requiredEntries,
                                             void*** backingOut) {
    if (!machineStatus || !backingOut || requiredEntries == 0 ||
        requiredEntries > PRODUCTION_MACHINE_SLOTS *
                              PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(machineStatus) +
                PRODUCTION_ITEM_LIST_OFFSET,
            sizeof(void*))) return false;
    void** backing = *reinterpret_cast<void***>(
        reinterpret_cast<unsigned char*>(machineStatus) +
        PRODUCTION_ITEM_LIST_OFFSET);
    // A level-one processor commits only the first eight ingredient entries.
    // Bytes for level-two slots are dormant/uncommitted storage and must not
    // be used to reject slot zero.  Validate only the selected slot's prefix.
    if (!backing || !NearbyIsReadable(
            backing, requiredEntries * sizeof(void*))) return false;
    *backingOut = backing;
    return true;
}

static bool ProductionForcesRankZero(u64 itemId) {
    if (itemId >= 0x330ccULL && itemId - 0x330ccULL <= 0x14ULL &&
        ((0x100401ULL >> (itemId - 0x330ccULL)) & 1ULL) != 0)
        return true;
    if (itemId >= 0x330eaULL && itemId - 0x330eaULL <= 0x3cULL &&
        ((0x1004010040100401ULL >> (itemId - 0x330eaULL)) & 1ULL) != 0)
        return true;
    return false;
}

static bool ProductionRejectOutputRankPreflight(
        const char* reason, size_t slot, size_t backingSlots,
        int itemIndex) {
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=rank_preflight result=FAIL "
        "reason=%s slot=%zu backing_slots=%zu "
        "item_index=%d native_reads_only=1 writes=0\n",
        reason ? reason : "rank_preflight_invalid", slot, backingSlots,
        itemIndex);
    return false;
}

static bool ProductionReadExpectedOutputRank(void* machineStatus,
                                             size_t slot, u64 itemId,
                                             int* rankOut) {
    if (!machineStatus || !rankOut || slot >= PRODUCTION_MACHINE_SLOTS ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(machineStatus) +
                PRODUCTION_ITEM_LIST_OFFSET,
            sizeof(void*)))
        return ProductionRejectOutputRankPreflight(
            "machine_item_storage_unreadable", slot, 0, -1);
    void** items = *reinterpret_cast<void***>(
            reinterpret_cast<unsigned char*>(machineStatus) +
            PRODUCTION_ITEM_LIST_OFFSET);
    // GimmickProcessPopItem loads exactly one pointer from status+0x2B8 and
    // indexes its fixed 3 x 8 backing array.  The adjacent +0x2C0/+0x2C8 bytes
    // are not vector end/capacity fields and must not be interpreted as such.
    // Only the prefix through the selected slot is committed.  In particular
    // a base (level-one) machine exposes eight valid entries, not 24.
    const size_t machineItemBackingSlots = (slot + 1) * 8;
    if (!items || !NearbyIsReadable(
            items, machineItemBackingSlots * sizeof(void*)))
        return ProductionRejectOutputRankPreflight(
            "machine_item_backing_unreadable", slot,
            machineItemBackingSlots, -1);
    int rank = 0;
    const size_t first = slot * 8;
    for (size_t index = first; index < first + 8; ++index) {
        if (!items[index]) continue;
        const unsigned char* item = reinterpret_cast<const unsigned char*>(
            items[index]);
        if (!NearbyIsReadable(item + NEARBY_ITEM_RANK_OFFSET, sizeof(int)))
            return ProductionRejectOutputRankPreflight(
                "machine_item_rank_unreadable", slot,
                machineItemBackingSlots, static_cast<int>(index));
        // The native completion helper reads only CItemStatus::rank here.  An
        // item-id/count validation is stronger than native and previously
        // turned otherwise transferable finished products into a global
        // output_rank_unreadable fault.
        const int itemRank = *reinterpret_cast<const int*>(
            item + NEARBY_ITEM_RANK_OFFSET);
        if (itemRank > rank) rank = itemRank;
    }
    *rankOut = ProductionForcesRankZero(itemId) ? 0 : rank;
    return true;
}

static bool ProductionWillResetInProcAfterSlot(
        void* machineStatus, size_t completingSlot,
        std::int64_t gameSecond, bool* resetOut,
        const char** failReasonOut = nullptr) {
    if (failReasonOut) *failReasonOut = nullptr;
    if (!machineStatus || !resetOut ||
        completingSlot >= PRODUCTION_MACHINE_SLOTS || gameSecond < 0) {
        if (failReasonOut) *failReasonOut = "inproc_probe_invalid_arguments";
        return false;
    }
    size_t slotLimit = 0;
    if (!ProductionReadMachineSlotLimit(machineStatus, &slotLimit) ||
        slotLimit == 0 || slotLimit > PRODUCTION_MACHINE_SLOTS ||
        completingSlot >= slotLimit) {
        if (failReasonOut) *failReasonOut = "inproc_probe_slot_limit_unreadable";
        return false;
    }
    // Exact GimmickGetInProcess semantics: inProc describes an unfinished
    // timer, not an output waiting for pickup.  Completed sibling slots do not
    // keep it set.  The completing slot is excluded because the helper is
    // about to clear it before this transition is published.
    for (size_t slot = 0; slot < slotLimit; ++slot) {
        if (slot == completingSlot) continue;
        const unsigned char* bytes =
            reinterpret_cast<const unsigned char*>(machineStatus);
        if (!NearbyIsReadable(
                bytes + PRODUCTION_TIME_VALUE_OFFSET + slot * sizeof(u64),
                sizeof(u64)) ||
            !NearbyIsReadable(
                bytes + PRODUCTION_CREATE_TIME_OFFSET + slot * sizeof(u64),
                sizeof(u64))) {
            if (failReasonOut)
                *failReasonOut = "inproc_probe_sibling_timer_unreadable";
            return false;
        }
        const u64 start = *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_TIME_VALUE_OFFSET + slot * sizeof(u64));
        const u64 duration = *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_CREATE_TIME_OFFSET + slot * sizeof(u64));
        if (!duration) continue;
        if (start > static_cast<u64>(INT64_MAX) ||
            duration > static_cast<u64>(INT64_MAX) ||
            gameSecond < static_cast<std::int64_t>(start) ||
            gameSecond - static_cast<std::int64_t>(start) <
                static_cast<std::int64_t>(duration)) {
            *resetOut = false;
            return true;
        }
    }
    u64 inProc = 0;
    if (!ProductionReadStatusMapValue(machineStatus, 0x18, "inProc",
                                      &inProc, true)) {
        if (failReasonOut) *failReasonOut = "inproc_probe_value_unreadable";
        return false;
    }
    *resetOut = static_cast<int>(inProc) != 0;
    return true;
}

static void ProductionFaultTransfer(const char* reason, void* item) {
    g_productionTransferFaulted.store(true, std::memory_order_release);
    if (item) {
        void* expected = nullptr;
        g_productionTransferQuarantine.compare_exchange_strong(expected, item);
    }
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=transfer_fault result=FAIL "
        "reason=%s quarantine=%d raw_pointer_cache=0 writes=0\n",
        reason, g_productionTransferQuarantine.load() ? 1 : 0);
}

static const ProductionStatusHit* ProductionFindStatusHit(
        const ProductionStatusHit* hits, size_t hitCount,
        const ProductionStableId& id) {
    for (size_t index = 0; index < hitCount; ++index) {
        if (ProductionIdEqual(hits[index].id, id)) return &hits[index];
    }
    return nullptr;
}

struct ProductionMergeTarget {
    void* stack;
    size_t slot;
    int before;
    int amount;
};

struct ProductionTransferPlan {
    void** vectorBegin;
    ProductionMergeTarget merges[PRODUCTION_CHEST_SLOTS];
    size_t mergeCount;
    size_t freeSlot;
    int rank;
    int remaining;
    bool usesFreeSlot;
    bool stackabilityKnown;
    bool stackable;
};

enum class ProductionChestPlanResult : unsigned char {
    Ready,
    CapacityInsufficient,
    DuplicateOwnership,
    Invalid
};

// Read the live output chest inventory and build the same multi-target plan as
// the native CommandItem delivery helper: distribute over every same-id/rank
// stack up to 999, then publish only the remainder into the first empty slot.
// No game state is written here.  If a matching live item is present it is a
// reliable pre-helper argument for the native read-only stackability query;
// otherwise an empty-slot-only plan does not need that property yet.
static ProductionChestPlanResult ProductionPlanChestDelivery(
        void* chestStatus, u64 itemId, int rank, int amount,
        bool stackabilityKnown, bool stackable,
        ProductionTransferPlan* plan) {
    if (!chestStatus || !plan || amount <= 0 ||
        !g_productionItemIsStackable)
        return ProductionChestPlanResult::Invalid;
    *plan = {};
    plan->freeSlot = PRODUCTION_CHEST_SLOTS;
    plan->rank = rank;
    plan->remaining = amount;
    plan->stackabilityKnown = stackabilityKnown;
    plan->stackable = stackable;
    NearbyRawPointerVector items = {};
    if (!NearbyReadRawInventoryRelaxed(chestStatus, &items))
        return ProductionChestPlanResult::Invalid;
    plan->vectorBegin = items.begin;
    const size_t effectiveSlots = (reinterpret_cast<uintptr_t>(items.end) -
        reinterpret_cast<uintptr_t>(items.begin)) / sizeof(void*);
    const size_t slotCount = effectiveSlots <= PRODUCTION_CHEST_SLOTS
        ? effectiveSlots : PRODUCTION_CHEST_SLOTS;
    size_t firstFree = PRODUCTION_CHEST_SLOTS;
    void* firstMatching = nullptr;
    void* uniqueItems[PRODUCTION_CHEST_SLOTS] = {};
    size_t uniqueItemCount = 0;
    for (size_t slot = 0; slot < slotCount; ++slot) {
        void* candidate = items.begin[slot];
        if (!candidate) {
            if (firstFree == PRODUCTION_CHEST_SLOTS) firstFree = slot;
            continue;
        }
        // A raw inventory slot owns one intrusive reference.  The same item
        // pointer appearing in two slots would make a multi-stack plan count
        // and mutate one object twice, so reject the whole transaction before
        // the irreversible output helper is entered.
        for (size_t seen = 0; seen < uniqueItemCount; ++seen) {
            if (uniqueItems[seen] == candidate)
                return ProductionChestPlanResult::DuplicateOwnership;
        }
        uniqueItems[uniqueItemCount++] = candidate;
        NearbyItemInfo info = {};
        if (!NearbyReadItem(candidate, &info))
            return ProductionChestPlanResult::Invalid;
        if (info.itemId == itemId && info.rank == rank && !firstMatching)
            firstMatching = candidate;
    }

    if (!plan->stackabilityKnown && firstMatching) {
        plan->stackabilityKnown = true;
        plan->stackable = g_productionItemIsStackable(firstMatching);
    }
    if (plan->stackabilityKnown && plan->stackable) {
        for (size_t slot = 0; slot < slotCount &&
                              plan->remaining > 0; ++slot) {
            void* candidate = items.begin[slot];
            if (!candidate) continue;
            NearbyItemInfo info = {};
            if (!NearbyReadItem(candidate, &info))
                return ProductionChestPlanResult::Invalid;
            if (info.itemId != itemId || info.rank != rank ||
                info.stackCount < 0 || info.stackCount >= 999) continue;
            const int capacity = 999 - info.stackCount;
            const int move = capacity < plan->remaining
                ? capacity : plan->remaining;
            ProductionMergeTarget& target =
                plan->merges[plan->mergeCount++];
            target.stack = candidate;
            target.slot = slot;
            target.before = info.stackCount;
            target.amount = move;
            plan->remaining -= move;
        }
    }
    if (plan->remaining > 0) {
        if (firstFree == PRODUCTION_CHEST_SLOTS)
            return ProductionChestPlanResult::CapacityInsufficient;
        plan->usesFreeSlot = true;
        plan->freeSlot = firstFree;
    }
    return ProductionChestPlanResult::Ready;
}

static bool ProductionTransferPlanUnchanged(
        const ProductionTransferPlan& before,
        const ProductionTransferPlan& after) {
    if (before.vectorBegin != after.vectorBegin ||
        before.mergeCount != after.mergeCount ||
        before.freeSlot != after.freeSlot || before.rank != after.rank ||
        before.remaining != after.remaining ||
        before.usesFreeSlot != after.usesFreeSlot ||
        (before.stackabilityKnown &&
         (!after.stackabilityKnown || before.stackable != after.stackable)))
        return false;
    for (size_t index = 0; index < before.mergeCount; ++index) {
        const ProductionMergeTarget& left = before.merges[index];
        const ProductionMergeTarget& right = after.merges[index];
        if (left.stack != right.stack || left.slot != right.slot ||
            left.before != right.before || left.amount != right.amount)
            return false;
    }
    return true;
}

static bool ProductionBindingMatchesTransfer(
        const ProductionBinding& live, const ProductionBinding& candidate) {
    return ProductionIdEqual(live.machine, candidate.machine) &&
           ProductionIdEqual(live.input, candidate.input) &&
           ProductionIdEqual(live.output, candidate.output) &&
           live.hasInput == candidate.hasInput &&
           live.hasOutput == candidate.hasOutput &&
           live.enabled == candidate.enabled &&
           live.committed == candidate.committed && live.committed &&
           live.enabled && live.hasOutput &&
           live.state == ProductionState::OutputReady;
}

static bool ProductionBindingMatchesInput(
        const ProductionBinding& live, const ProductionBinding& candidate) {
    return ProductionIdEqual(live.machine, candidate.machine) &&
           ProductionIdEqual(live.input, candidate.input) &&
           ProductionIdEqual(live.output, candidate.output) &&
           live.hasInput == candidate.hasInput && live.hasInput &&
           live.hasOutput == candidate.hasOutput && live.hasOutput &&
           live.enabled == candidate.enabled && live.enabled &&
           live.committed == candidate.committed && live.committed &&
           (live.state == ProductionState::WaitingInput ||
            (live.state == ProductionState::Running &&
             live.dueKind == ProductionDueKind::InputRetry));
}

struct ProductionDueWork {
    ProductionBinding binding;
    u64 revision;
    u64 requiredCompletedSequence;
    u64 loadGeneration;
    u64 worldEpoch;
    bool outputPriority;
};

// Revalidation used only after an input reservation is held.  Unlike the
// pre-reservation gate, transferInFlight must remain true.  This is called
// after native helpers/notifications before another write is published.
static const char* ProductionReservedInputTransactionGate(
        const ProductionDueWork& work,
        const ProductionBinding& candidate) {
    if (!ProductionFeatureIsEnabled()) return "feature_disabled";
    if (!g_productionWorldContextActive.load(std::memory_order_acquire))
        return "world_inactive";
    if (g_productionRegistryScanRequested.load(std::memory_order_acquire) ||
        g_productionScanInProgress.load(std::memory_order_acquire) ||
        g_productionScanActive || g_productionScanning)
        return "scan_in_progress";
    if (g_productionWorldScheduleEpoch.load(std::memory_order_acquire) !=
            work.worldEpoch) return "world_epoch_changed";
    if (g_productionBindingMutationInProgress.load(
            std::memory_order_acquire)) return "binding_mutation_in_progress";
    if (g_productionBindingRevision.load(std::memory_order_acquire) !=
            work.revision) return "binding_revision_changed";
    if (g_productionTransferMutationSequence.load(
            std::memory_order_acquire) != work.requiredCompletedSequence)
        return "callback_requirement_changed";
    if (g_productionMainCallbackCompletedSequence.load(
            std::memory_order_acquire) < work.requiredCompletedSequence)
        return "completed_callback_pending";
    if (GetTickCount64() < g_productionTransferNotBeforeTick.load(
            std::memory_order_acquire)) return "not_before_pending";
    if (g_autoPetLoadInFlightGeneration.load(std::memory_order_acquire) != 0)
        return "load_in_flight";
    if (g_autoPetLoadCompletionGeneration.load(std::memory_order_acquire) !=
            work.loadGeneration) return "load_completion_changed";
    if (g_autoPetLoadReadyGeneration.load(std::memory_order_acquire) !=
            work.loadGeneration) return "load_ready_changed";
    if (g_autoPetHandledLoadGeneration.load(std::memory_order_acquire) !=
            work.loadGeneration) return "load_handled_changed";
    if (!g_productionTransferInFlight.load(std::memory_order_acquire))
        return "reservation_lost";
    bool bindingCurrent = false;
    AcquireSRWLockShared(&g_productionBindingLock);
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        if (ProductionBindingMatchesInput(g_productionBindings[index],
                                          candidate)) {
            bindingCurrent = true;
            break;
        }
    }
    ReleaseSRWLockShared(&g_productionBindingLock);
    return bindingCurrent ? "reserved" : "binding_state_changed";
}

static void ProductionArmPostTransactionGate(u64 callbackSequence) {
    // The transaction runs before original update.  Publishing its callback
    // sequence is sufficient: the following pre-update is eligible only after
    // that original callback has completed.  A successful output must not add
    // an unrelated five-second wall delay before its next-callback InputRetry.
    u64 observed = g_productionTransferMutationSequence.load(
        std::memory_order_acquire);
    for (unsigned attempt = 0; attempt < 64 && observed < callbackSequence;
         ++attempt) {
        if (g_productionTransferMutationSequence.compare_exchange_weak(
                observed, callbackSequence, std::memory_order_acq_rel,
                std::memory_order_acquire)) break;
    }
    if (callbackSequence > g_productionTransferMutationSequence.load(
            std::memory_order_acquire))
        g_productionTransferMutationSequence.store(
            callbackSequence, std::memory_order_release);
    g_productionMutationDeferredLogged.store(false,
                                              std::memory_order_release);
}

static bool ProductionFinalizeClearedMachineSlot(
        void* machineStatus, const ProductionBinding& binding, size_t slot,
        bool resetInProc) {
    if (!machineStatus || slot >= PRODUCTION_MACHINE_SLOTS ||
        !ProductionValidateMachineStatusForNativeTransaction(
            machineStatus, &binding.machine)) return false;
    ProductionStableId identity = {};
    if (!ProductionReadGimmickIdentity(machineStatus, &identity) ||
        !ProductionIdEqual(identity, binding.machine)) return false;
    const unsigned char* bytes =
        reinterpret_cast<const unsigned char*>(machineStatus);
    if (!NearbyIsReadable(
            bytes + PRODUCTION_TIME_VALUE_OFFSET + slot * sizeof(u64),
            sizeof(u64)) ||
        !NearbyIsReadable(
            bytes + PRODUCTION_CREATE_TIME_OFFSET + slot * sizeof(u64),
            sizeof(u64)) ||
        *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_TIME_VALUE_OFFSET + slot * sizeof(u64)) != 0 ||
        *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_CREATE_TIME_OFFSET + slot * sizeof(u64)) != 0)
        return false;
    // v1.1.23: 恢复参考源的 inProc 重置逻辑。
    // v1.1.22 的 g_productionStatusEventNotify(machineStatus) 传参错误——
    // 应传事件节点(node+0x18)而非机器状态对象，导致 inProc 从不归零。
    // 参考源正确写法：ProductionSetStatusInt(machineStatus, "inProc", 0)，
    // 内部先写 0x18 map 值，再查 event owner → 查事件节点 → notify(node+0x18)。
    // 若 notify 路径不可用，回退到 silent 写入（只写值不通知）。
    if (!resetInProc) return true;
    if (ProductionCanSetStatusInt(machineStatus))
        return ProductionSetStatusInt(machineStatus, "inProc", 0);
    const bool silentOk = ProductionSetStatusIntSilent(
        machineStatus, "inProc", 0);
    ProductionLog(
        "[PRODAUTO] seq=0 txn=0 event=inproc_reset "
        "result=%s reason=%s mode=%s device_map_id=%llu device_id=%llu "
        "slot=%zu raw_pointer_cache=0 writes=%d\n",
        silentOk ? "PASS" : "FAIL",
        silentOk ? "none" : "inproc_silent_reset_failed",
        silentOk ? "silent_no_notify" : "unavailable",
        static_cast<unsigned long long>(binding.machine.mapId),
        static_cast<unsigned long long>(binding.machine.uniqueId),
        slot, silentOk ? 1 : 0);
    return silentOk;
}

static bool ProductionRollbackChestMerges(
        const ProductionTransferPlan& plan, size_t applied) {
    bool rollbackOk = applied <= plan.mergeCount;
    while (applied > 0) {
        const ProductionMergeTarget& target = plan.merges[--applied];
        int current = 0;
        if (!NearbyReadStackCount(target.stack, &current)) {
            rollbackOk = false;
            continue;
        }
        if (current != target.before)
            g_nearbyItemAdjust(target.stack, target.before - current);
        if (!NearbyReadStackCount(target.stack, &current) ||
            current != target.before) rollbackOk = false;
    }
    return rollbackOk;
}

static bool ProductionRestoreProducedCount(void* item, int amount) {
    int current = 0;
    if (!item || !NearbyReadStackCount(item, &current)) return false;
    if (current != amount) g_nearbyItemAdjust(item, amount - current);
    return NearbyReadStackCount(item, &current) && current == amount;
}

static bool ProductionClassifyMachineAfterTransfer(
        void* machineStatus, std::int64_t gameSecond,
        ProductionState* stateOut, ProductionReason* reasonOut) {
    if (!stateOut || !reasonOut) return false;
    ProductionObjectSnapshot current = {};
    if (!ProductionReadObject(machineStatus, &current, false) || !current.machine ||
        !current.processValid || current.slotLimit == 0 ||
        current.slotLimit > PRODUCTION_MACHINE_SLOTS) return false;
    bool running = false;
    bool ready = false;
    for (size_t slot = 0; slot < current.slotLimit; ++slot) {
        if (!current.start[slot] || !current.duration[slot] ||
            !current.outputItemId[slot] || current.outputCount[slot] <= 0)
            continue;
        if (gameSecond >= 0 && current.start[slot] <= INT64_MAX &&
            current.duration[slot] <= INT64_MAX &&
            gameSecond >= static_cast<std::int64_t>(current.start[slot]) &&
            gameSecond - static_cast<std::int64_t>(current.start[slot]) >=
                static_cast<std::int64_t>(current.duration[slot]))
            ready = true;
        else
            running = true;
    }
    if (ready) {
        *stateOut = ProductionState::OutputReady;
        *reasonOut = ProductionReason::OutputNeedsTransfer;
    } else if (running) {
        *stateOut = ProductionState::Running;
        *reasonOut = ProductionReason::RunningNativeTimer;
    } else if (ProductionIsPassiveMachine(current.gimmickId)) {
        // Passive machines: if no timer-based classification matched, check
        // for remaining output items without timers.  When the output was
        // just drained, fall back to WaitingInput so the machine can be fed
        // again instead of being stuck in a permanent OutputReady retry.
        bool passiveHasOutput = false;
        for (size_t slot = 0; slot < current.slotLimit; ++slot) {
            if (current.outputItemId[slot] && current.outputCount[slot] > 0) {
                passiveHasOutput = true;
                break;
            }
        }
        if (passiveHasOutput) {
            *stateOut = ProductionState::OutputReady;
            *reasonOut = ProductionReason::OutputNeedsTransfer;
        } else {
            *stateOut = ProductionState::WaitingInput;
            *reasonOut = ProductionReason::WaitingIngredients;
        }
    } else {
        *stateOut = ProductionState::WaitingInput;
        *reasonOut = ProductionReason::WaitingIngredients;
    }
    return true;
}

static void ProductionLogTransfer(
        u64 txn, u64 callbackSequence, const ProductionBinding& binding,
        const ProductionObjectSnapshot* machine, size_t slot,
        u64 itemId, int amount, const char* outcome, const char* reason,
        const char* transferCase, const char* rollback,
        ProductionState stateAfter, const char* fromState,
        const char* toState, int removedOverride = -1,
        int writtenOverride = -1, int writesOverride = -1,
        std::int64_t gameSecondOverride = -1, int outputRankOverride = -1) {
    char inputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    char outputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    strcpy_s(inputName, binding.hasInput ? "unresolved" : "unbound");
    strcpy_s(outputName, binding.hasOutput ? "unresolved" : "unbound");
    AcquireSRWLockShared(&g_productionPublishLock);
    for (size_t index = 0; index < g_productionPublished.chestCount; ++index) {
        const ProductionObjectSnapshot& chest =
            g_productionPublished.chests[index];
        if (binding.hasInput && ProductionIdEqual(binding.input, chest.id))
            ProductionCopyLogToken(chest.name, inputName, sizeof(inputName));
        if (binding.hasOutput && ProductionIdEqual(binding.output, chest.id))
            ProductionCopyLogToken(chest.name, outputName, sizeof(outputName));
    }
    ReleaseSRWLockShared(&g_productionPublishLock);
    const u64 recipeId =
        machine ? ProductionInferRecipeId(machine, slot) : 0;
    const bool passed = strcmp(outcome, "PASS") == 0;
    const int removed = removedOverride >= 0
        ? removedOverride : (passed ? amount : 0);
    // -2 is a deliberate telemetry value for a failed rollback whose exact
    // chest residue cannot be proven; -1 remains the ordinary default.
    const int written = writtenOverride == -2
        ? -1 : (writtenOverride >= 0
            ? writtenOverride : (passed ? amount : 0));
    const int writes = writesOverride >= 0
        ? writesOverride : (passed ? 1 : 0);
    const std::int64_t gameSecond = gameSecondOverride >= 0
        ? gameSecondOverride
        : (g_productionScheduleGameSecondValid
               ? g_productionScheduleGameSecondObserved : -1);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=%llu event=transfer action=output "
        "callback_sequence=%llu "
        "device_map_id=%llu device_id=%llu input_map_id=%llu input_id=%llu "
        "output_map_id=%llu output_id=%llu input_name=%s output_name=%s "
        "due_kind=%s game_second=%lld machine_type=%llu module=%s "
        "slot=%zu slot_limit=%zu recipe=%llu planned=%d output_item=%llu "
        "output_rank=%d removed=%d written=%d rollback=%s "
        "state=%s from=%s to=%s result=%s reason=%s case=%s "
        "raw_pointer_cache=0 writes=%d\n",
        static_cast<unsigned long long>(txn),
        static_cast<unsigned long long>(callbackSequence),
        static_cast<unsigned long long>(binding.machine.mapId),
        static_cast<unsigned long long>(binding.machine.uniqueId),
        static_cast<unsigned long long>(binding.input.mapId),
        static_cast<unsigned long long>(binding.input.uniqueId),
        static_cast<unsigned long long>(binding.output.mapId),
        static_cast<unsigned long long>(binding.output.uniqueId),
        inputName, outputName, ProductionDueKindName(binding.dueKind),
        static_cast<long long>(gameSecond),
        static_cast<unsigned long long>(machine ? machine->gimmickId : 0),
        machine ? machine->module : "unresolved", slot,
        machine ? machine->slotLimit : 0,
        static_cast<unsigned long long>(recipeId), amount,
        static_cast<unsigned long long>(itemId), outputRankOverride,
        removed, written, rollback, ProductionStateName(stateAfter),
        fromState, toState, outcome, reason, transferCase, writes);
}

// Attempt the verified output transfer for one binding.  Returns true when
// every finished slot was delivered; false otherwise (capacity wait or a
// latched fault).  Only runs on the game main thread inside the world
// callback with freshly resolved status pointers.
static bool ProductionAttemptOutputTransfer(
        void* machineStatus, void* outputChestStatus,
        const ProductionBinding& binding,
        const ProductionObjectSnapshot* machine,
        u64 callbackSequence, std::int64_t gameSecond, bool* transferredAny,
        bool* capacityWait, bool* mutationAttempted,
        const char** failReasonOut = nullptr) {
    if (failReasonOut) *failReasonOut = nullptr;
    if (transferredAny) *transferredAny = false;
    if (capacityWait) *capacityWait = false;
    if (mutationAttempted) *mutationAttempted = false;
    if (!machineStatus || !outputChestStatus || !machine ||
        !machine->processValid || machine->slotLimit == 0 ||
        machine->slotLimit > PRODUCTION_MACHINE_SLOTS ||
        !g_productionOutputHelper || !g_productionItemIsStackable ||
        !g_nearbyItemAdjust ||
        !g_nearbyIntrusiveRelease ||
        !g_productionStatusIntLookup || !g_productionStatusEventLookup ||
        !g_productionStatusEventNotify ||
        g_productionTransferFaulted.load(std::memory_order_acquire)) {
        if (failReasonOut) *failReasonOut = "output_preconditions_unavailable";
        return false;
    }
    ProductionStableId machineIdentity = {};
    ProductionStableId outputIdentity = {};
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=output_preflight_begin "
        "result=INCOMPLETE reason=none stage=identity "
        "callback_sequence=%llu device_map_id=%llu device_id=%llu "
        "output_map_id=%llu output_id=%llu game_second=%lld "
        "raw_pointer_cache=0 writes=0\n",
        static_cast<unsigned long long>(callbackSequence),
        static_cast<unsigned long long>(binding.machine.mapId),
        static_cast<unsigned long long>(binding.machine.uniqueId),
        static_cast<unsigned long long>(binding.output.mapId),
        static_cast<unsigned long long>(binding.output.uniqueId),
        static_cast<long long>(gameSecond));
    if (!ProductionReadGimmickIdentity(machineStatus, &machineIdentity) ||
        !ProductionReadGimmickIdentity(outputChestStatus, &outputIdentity) ||
        !ProductionIdEqual(machineIdentity, binding.machine) ||
        !ProductionIdEqual(outputIdentity, binding.output)) {
        if (failReasonOut) *failReasonOut = "output_identity_mismatch";
        return false;
    }
    ProductionMachineStatusValidation machineValidation = {};
    const bool machineReady =
        ProductionValidateMachineStatusForNativeTransaction(
            machineStatus, &binding.machine, &machineValidation);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=output_preflight_begin "
        "result=%s reason=%s stage=live_component "
        "callback_sequence=%llu device_map_id=%llu device_id=%llu "
        "output_map_id=%llu output_id=%llu game_second=%lld "
        "component_roundtrip=%d object_link=%d "
        "raw_pointer_cache=0 writes=0\n",
        machineReady ? "PASS" : "FAIL",
        machineReady ? "none" : "machine_status_invalid",
        static_cast<unsigned long long>(callbackSequence),
        static_cast<unsigned long long>(binding.machine.mapId),
        static_cast<unsigned long long>(binding.machine.uniqueId),
        static_cast<unsigned long long>(binding.output.mapId),
        static_cast<unsigned long long>(binding.output.uniqueId),
        static_cast<long long>(gameSecond),
        machineValidation.componentRoundTrip ? 1 : 0,
        machineValidation.objectLinkReadable ? 1 : 0);
    if (!machineReady) {
        if (failReasonOut) *failReasonOut = "machine_status_invalid";
        return false;
    }
    for (size_t slot = 0; slot < machine->slotLimit; ++slot) {
        const unsigned char* statusBytes =
            reinterpret_cast<const unsigned char*>(machineStatus);
        if (!NearbyIsReadable(
                statusBytes + PRODUCTION_TIME_VALUE_OFFSET + slot * 8,
                sizeof(u64)) ||
            !NearbyIsReadable(
                statusBytes + PRODUCTION_CREATE_TIME_OFFSET + slot * 8,
                sizeof(u64)))
            return false;
        const u64 start = *reinterpret_cast<const u64*>(
            statusBytes + PRODUCTION_TIME_VALUE_OFFSET + slot * 8);
        const u64 duration = *reinterpret_cast<const u64*>(
            statusBytes + PRODUCTION_CREATE_TIME_OFFSET + slot * 8);
        ProductionLog("[PRODAUTO] seq=%llu txn=0 event=output_slot_scan device_map_id=%llu device_id=%llu slot=%zu start=%llu duration=%llu game_second=%lld gimmickId=%llu\n", static_cast<unsigned long long>(callbackSequence), static_cast<unsigned long long>(binding.machine.mapId), static_cast<unsigned long long>(binding.machine.uniqueId), slot, static_cast<unsigned long long>(start), static_cast<unsigned long long>(duration), static_cast<long long>(gameSecond), static_cast<unsigned long long>(machine->gimmickId));
        if (!start || !duration) {
            // Passive machines (beehive/sap extractor/sericulture box) may
            // have output items without timers.  Check output maps directly.
            if (ProductionIsPassiveMachine(machine->gimmickId) &&
                ProductionSlotHasOutputNoTimer(machineStatus, slot)) {
                // Fall through to read itemId/countValue below and transfer.
            } else {
                continue;
            }
        }
        const bool finished = (!start || !duration) ||
            (gameSecond >= 0 && start <= INT64_MAX && duration <= INT64_MAX &&
            gameSecond >= static_cast<std::int64_t>(start) &&
            gameSecond - static_cast<std::int64_t>(start) >=
                static_cast<std::int64_t>(duration));
        if (!finished) {
            ProductionLog("[PRODAUTO] seq=%llu txn=0 event=output_slot_not_finished device_map_id=%llu device_id=%llu slot=%zu start=%llu duration=%llu game_second=%lld gimmickId=%llu\n", static_cast<unsigned long long>(callbackSequence), static_cast<unsigned long long>(binding.machine.mapId), static_cast<unsigned long long>(binding.machine.uniqueId), slot, static_cast<unsigned long long>(start), static_cast<unsigned long long>(duration), static_cast<long long>(gameSecond), static_cast<unsigned long long>(machine->gimmickId));
            continue; // still processing
        }
        char idKey[8] = {};
        char countKey[9] = {};
        _snprintf_s(idKey, sizeof(idKey), _TRUNCATE, "itemID%zu", slot);
        _snprintf_s(countKey, sizeof(countKey), _TRUNCATE, "itemVal%zu", slot);
        u64 itemId = 0;
        u64 countValue = 0;
        if (!ProductionReadStatusMapValue(machineStatus, 0x58, idKey, &itemId,
                                          false) ||
            !ProductionReadStatusMapValue(machineStatus, 0x18, countKey,
                                          &countValue, true) ||
            !itemId || countValue <= 0 || countValue > 999) {
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            ProductionFaultTransfer("slot_map_unreadable", nullptr);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, 0, "FAIL",
                "slot_map_unreadable", "machine_output_read_failed",
                "failed", ProductionState::Faulted,
                ProductionStateName(binding.state),
                ProductionStateName(ProductionState::Faulted));
            return false;
        }
        ProductionLog("[PRODAUTO] seq=%llu txn=0 event=output_slot_item_read device_map_id=%llu device_id=%llu slot=%zu itemID=%llu itemVal=%llu gimmickId=%llu\n", static_cast<unsigned long long>(callbackSequence), static_cast<unsigned long long>(binding.machine.mapId), static_cast<unsigned long long>(binding.machine.uniqueId), slot, static_cast<unsigned long long>(itemId), static_cast<unsigned long long>(countValue), static_cast<unsigned long long>(machine->gimmickId));
        const int amount = static_cast<int>(countValue);
        int expectedRank = 0;
        if (!ProductionReadExpectedOutputRank(machineStatus, slot, itemId,
                                              &expectedRank)) {
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            ProductionFaultTransfer("output_rank_unreadable", nullptr);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL",
                "output_rank_unreadable", "machine_output_read_failed",
                "not_needed", ProductionState::Faulted,
                ProductionStateName(binding.state),
                ProductionStateName(ProductionState::Faulted), 0, 0, 0);
            return false;
        }
        ProductionTransferPlan plan = {};
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=output_chest_plan_begin "
            "result=INCOMPLETE reason=none callback_sequence=%llu "
            "device_map_id=%llu device_id=%llu output_map_id=%llu "
            "output_id=%llu slot=%zu item_id=%llu rank=%d count=%d "
            "game_second=%lld raw_pointer_cache=0 writes=0\n",
            static_cast<unsigned long long>(callbackSequence),
            static_cast<unsigned long long>(binding.machine.mapId),
            static_cast<unsigned long long>(binding.machine.uniqueId),
            static_cast<unsigned long long>(binding.output.mapId),
            static_cast<unsigned long long>(binding.output.uniqueId),
            slot, static_cast<unsigned long long>(itemId), expectedRank,
            amount, static_cast<long long>(gameSecond));
        const ProductionChestPlanResult planResult =
            ProductionPlanChestDelivery(outputChestStatus, itemId,
                                        expectedRank, amount, false, false,
                                        &plan);
        if (planResult != ProductionChestPlanResult::Ready) {
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const bool capacityOnly =
                planResult ==
                    ProductionChestPlanResult::CapacityInsufficient;
            const char* reason = capacityOnly
                ? "output_capacity_insufficient"
                : (planResult ==
                       ProductionChestPlanResult::DuplicateOwnership
                    ? "output_inventory_alias"
                    : "output_inventory_invalid");
            if (!capacityOnly) ProductionFaultTransfer(reason, nullptr);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount,
                capacityOnly ? "INCOMPLETE" : "FAIL", reason,
                capacityOnly ? "output_full_wait" : "chest_plan_invalid",
                "not_needed", capacityOnly
                    ? ProductionState::WaitingOutputSpace
                    : ProductionState::Faulted,
                ProductionStateName(binding.state),
                ProductionStateName(capacityOnly
                    ? ProductionState::WaitingOutputSpace
                    : ProductionState::Faulted),
                0, 0, 0, gameSecond, expectedRank);
            if (capacityOnly && capacityWait) *capacityWait = true;
            return false;
        }
        bool resetInProc = false;
        const char* inprocProbeReason = nullptr;
        const bool inprocProbeOk = ProductionWillResetInProcAfterSlot(
            machineStatus, slot, gameSecond, &resetInProc,
            &inprocProbeReason);
        const bool inprocNotifyReady =
            ProductionCanSetStatusInt(machineStatus);
        const bool inprocSilentReady =
            ProductionCanSetStatusIntSilent(machineStatus);
        const char* inprocMode = inprocNotifyReady ? "notify" :
            (inprocSilentReady ? "silent_no_notify" : "unavailable");
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=output_inproc_preflight "
            "result=%s reason=%s callback_sequence=%llu "
            "device_map_id=%llu device_id=%llu slot=%zu reset=%d "
            "mode=%s raw_pointer_cache=0 writes=0\n",
            inprocProbeOk &&
                    (!resetInProc || inprocNotifyReady || inprocSilentReady)
                ? "PASS" : "FAIL",
            !inprocProbeOk
                ? (inprocProbeReason ? inprocProbeReason :
                                       "inproc_probe_unavailable")
                : (resetInProc ? "none" : "none"),
            static_cast<unsigned long long>(callbackSequence),
            static_cast<unsigned long long>(binding.machine.mapId),
            static_cast<unsigned long long>(binding.machine.uniqueId),
            slot, resetInProc ? 1 : 0, inprocMode);
        if (!inprocProbeOk ||
            (resetInProc && !inprocNotifyReady && !inprocSilentReady)) {
            if (failReasonOut) {
                *failReasonOut = !inprocProbeOk
                    ? "inproc_probe_unavailable"
                    : "inproc_reset_unavailable";
            }
            return false;
        }
        const char* beforeState = ProductionStateName(binding.state);
        void* item = nullptr;
        if (mutationAttempted) *mutationAttempted = true;
        g_productionPreUpdateStage.store(4, std::memory_order_release);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=transfer_begin "
            "result=INCOMPLETE reason=none phase=output_helper "
            "device_map_id=%llu device_id=%llu output_map_id=%llu "
            "output_id=%llu slot=%zu item_id=%llu planned=%d "
            "raw_pointer_cache=0 writes=0\n",
            static_cast<unsigned long long>(binding.machine.mapId),
            static_cast<unsigned long long>(binding.machine.uniqueId),
            static_cast<unsigned long long>(binding.output.mapId),
            static_cast<unsigned long long>(binding.output.uniqueId),
            slot, static_cast<unsigned long long>(itemId), amount);
        g_productionOutputHelper(&item, machineStatus, static_cast<int>(slot));
        g_productionPreUpdateStage.store(5, std::memory_order_release);
        const bool slotCleared =
            NearbyIsReadable(
                statusBytes + PRODUCTION_TIME_VALUE_OFFSET + slot * 8,
                sizeof(u64)) &&
            NearbyIsReadable(
                statusBytes + PRODUCTION_CREATE_TIME_OFFSET + slot * 8,
                sizeof(u64)) &&
            *reinterpret_cast<const u64*>(
                statusBytes + PRODUCTION_TIME_VALUE_OFFSET + slot * 8) == 0 &&
            *reinterpret_cast<const u64*>(
                statusBytes + PRODUCTION_CREATE_TIME_OFFSET + slot * 8) == 0;
        if (!slotCleared) {
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            ProductionFaultTransfer("machine_slot_not_cleared", item);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL",
                "machine_slot_not_cleared", "machine_pop_failed", "failed",
                ProductionState::Faulted, beforeState,
                ProductionStateName(ProductionState::Faulted), 0, 0, 1);
            return false;
        }
        const auto finalizeClearedSlot = [&]() {
            g_productionPreUpdateStage.store(7, std::memory_order_release);
            return ProductionFinalizeClearedMachineSlot(
                machineStatus, binding, slot, resetInProc);
        };
        if (!item ||
            !NearbyIsReadable(item, NEARBY_ITEM_RANK_OFFSET + sizeof(int))) {
            const bool finalized = finalizeClearedSlot();
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const char* reason = finalized ? "output_helper_no_item" :
                                             "inproc_recovery_failed";
            ProductionFaultTransfer(reason, item);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL",
                reason, "machine_pop_failed",
                finalized ? "inproc_finalized" : "failed",
                ProductionState::Faulted, beforeState,
                ProductionStateName(ProductionState::Faulted), amount, 0, 1);
            return false;
        }
        NearbyItemInfo info = {};
        if (!NearbyReadItem(item, &info) || info.itemId != itemId ||
            info.stackCount != amount || info.rank != expectedRank) {
            const bool finalized = finalizeClearedSlot();
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const char* reason = finalized ? "output_item_mismatch" :
                                             "inproc_recovery_failed";
            ProductionFaultTransfer(reason, item);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL",
                reason, "machine_pop_failed",
                finalized ? "inproc_finalized" : "failed",
                ProductionState::Faulted, beforeState,
                ProductionStateName(ProductionState::Faulted), amount, 0, 1);
            return false;
        }
        ProductionStableId machineIdentityAfter = {};
        ProductionStableId outputIdentityAfter = {};
        ProductionTransferPlan refreshedPlan = {};
        if (!ProductionValidateMachineStatusForNativeTransaction(
                machineStatus, &binding.machine) ||
            !ProductionReadGimmickIdentity(machineStatus,
                                            &machineIdentityAfter) ||
            !ProductionReadGimmickIdentity(outputChestStatus,
                                            &outputIdentityAfter) ||
            !ProductionIdEqual(machineIdentityAfter, binding.machine) ||
            !ProductionIdEqual(outputIdentityAfter, binding.output) ||
            ProductionPlanChestDelivery(
                outputChestStatus, itemId, info.rank, amount, true,
                g_productionItemIsStackable(item), &refreshedPlan) !=
                ProductionChestPlanResult::Ready ||
             !ProductionTransferPlanUnchanged(plan, refreshedPlan)) {
            const bool finalized = finalizeClearedSlot();
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const char* reason = finalized
                ? "post_helper_revalidation_failed" :
                  "inproc_recovery_failed";
            ProductionFaultTransfer(reason, item);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL",
                reason, "chest_plan_changed",
                finalized ? "inproc_finalized" : "failed",
                ProductionState::Faulted, beforeState,
                ProductionStateName(ProductionState::Faulted), amount, 0, 1);
            return false;
        }
        plan = refreshedPlan;

        g_productionPreUpdateStage.store(6, std::memory_order_release);
        size_t appliedMerges = 0;
        bool commitFailed = false;
        for (; appliedMerges < plan.mergeCount; ++appliedMerges) {
            const ProductionMergeTarget& target =
                plan.merges[appliedMerges];
            g_nearbyItemAdjust(target.stack, target.amount);
            int afterCount = 0;
            if (!NearbyReadStackCount(target.stack, &afterCount) ||
                afterCount != target.before + target.amount) {
                ++appliedMerges; // include the possibly-written current target
                commitFailed = true;
                break;
            }
        }
        if (commitFailed) {
            const bool rollbackOk =
                ProductionRollbackChestMerges(plan, appliedMerges) &&
                ProductionRestoreProducedCount(item, amount);
            const bool finalized = finalizeClearedSlot();
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const char* reason = finalized ? "chest_merge_verify_failed" :
                                             "inproc_recovery_failed";
            ProductionFaultTransfer(reason, item);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL", reason,
                "chest_merge_failed",
                rollbackOk && finalized ? "verified" : "failed",
                ProductionState::Faulted, beforeState,
                ProductionStateName(ProductionState::Faulted), amount,
                rollbackOk ? 0 : -2, 1);
            return false;
        }

        const int sourceDelta = plan.remaining - amount;
        if (sourceDelta != 0) g_nearbyItemAdjust(item, sourceDelta);
        int sourceCount = 0;
        if (!NearbyReadStackCount(item, &sourceCount) ||
            sourceCount != plan.remaining) {
            const bool rollbackOk =
                ProductionRestoreProducedCount(item, amount) &&
                ProductionRollbackChestMerges(plan, appliedMerges);
            const bool finalized = finalizeClearedSlot();
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const char* reason = finalized
                ? "popped_item_count_adjust_failed" :
                  "inproc_recovery_failed";
            ProductionFaultTransfer(reason, item);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL", reason,
                "chest_merge_failed",
                rollbackOk && finalized ? "verified" : "failed",
                ProductionState::Faulted, beforeState,
                ProductionStateName(ProductionState::Faulted), amount,
                rollbackOk ? 0 : -2, 1);
            return false;
        }

        void** freeSlot = nullptr;
        bool slotPublished = false;
        bool slotRollbackSafe = true;
        if (plan.usesFreeSlot) {
            if (!plan.vectorBegin || plan.freeSlot >= PRODUCTION_CHEST_SLOTS)
                commitFailed = true;
            else
                freeSlot = &plan.vectorBegin[plan.freeSlot];
            if (!commitFailed &&
                (!NearbyIsReadable(freeSlot, sizeof(void*)) ||
                 *freeSlot != nullptr)) commitFailed = true;
            if (!commitFailed) {
                NearbyAddReference(item);
                void* observed = InterlockedCompareExchangePointer(
                    reinterpret_cast<void* volatile*>(freeSlot), item,
                    nullptr);
                if (observed != nullptr) {
                    NearbyReleaseOwnedReference(item); // undo slot AddRef
                    commitFailed = true;
                } else {
                    slotPublished = NearbyIsReadable(freeSlot, sizeof(void*)) &&
                                    *freeSlot == item;
                    if (!slotPublished) {
                        void* removed = InterlockedCompareExchangePointer(
                            reinterpret_cast<void* volatile*>(freeSlot),
                            nullptr, item);
                        if (removed == item)
                            NearbyReleaseOwnedReference(item); // undo slot AddRef
                        else
                            slotRollbackSafe = false;
                    }
                }
            }
        }
        if (commitFailed || (plan.usesFreeSlot && !slotPublished)) {
            const bool sourceRestored = slotRollbackSafe &&
                ProductionRestoreProducedCount(item, amount);
            const bool rollbackOk = sourceRestored &&
                ProductionRollbackChestMerges(plan, appliedMerges);
            const bool finalized = finalizeClearedSlot();
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const char* reason = finalized
                ? (commitFailed ? "chest_slot_precommit_changed" :
                                  "chest_slot_publish_unverified")
                : "inproc_recovery_failed";
            ProductionFaultTransfer(reason, item);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL", reason,
                "chest_insert_failed",
                rollbackOk && finalized ? "verified" : "failed",
                ProductionState::Faulted, beforeState,
                ProductionStateName(ProductionState::Faulted), amount,
                rollbackOk ? 0 : -2, 1);
            return false;
        }

        if (!finalizeClearedSlot()) {
            const u64 txn = g_productionTxnSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            ProductionFaultTransfer("inproc_finalize_failed", item);
            ProductionLogTransfer(
                txn, callbackSequence, binding, machine, slot, itemId, amount, "FAIL",
                "inproc_finalize_failed",
                plan.usesFreeSlot ? "empty_slot_insert" :
                                    "merge_existing_stack",
                "failed", ProductionState::Faulted, beforeState,
                ProductionStateName(ProductionState::Faulted), amount,
                amount, 1);
            return false;
        }
        NearbyReleaseOwnedReference(item);
        if (transferredAny) *transferredAny = true;
        ProductionState stateAfter = ProductionState::WaitingInput;
        ProductionReason reasonAfter = ProductionReason::WaitingIngredients;
        const bool stateClassified = ProductionClassifyMachineAfterTransfer(
            machineStatus, gameSecond, &stateAfter, &reasonAfter);
        if (!stateClassified) {
            stateAfter = ProductionState::Faulted;
            reasonAfter = ProductionReason::RegistryInvalid;
            ProductionFaultTransfer("post_transfer_state_unreadable", nullptr);
        }
        const char* transferCase = plan.usesFreeSlot
            ? (plan.mergeCount ? "split_merge_and_insert" :
                                 "empty_slot_insert")
            : "merge_existing_stack";
        const u64 txn = g_productionTxnSequence.fetch_add(
            1, std::memory_order_relaxed) + 1;
        ProductionLogTransfer(
            txn, callbackSequence, binding, machine, slot, itemId, amount,
            stateClassified ? "PASS" : "FAIL",
            stateClassified ? "none" : "post_transfer_state_unreadable",
            transferCase, "not_needed", stateAfter,
            beforeState, ProductionStateName(stateAfter), amount, amount, 1,
            gameSecond, expectedRank);
        return stateClassified;
    }
    if (failReasonOut) *failReasonOut = "no_finished_slot";
    return false;
}

// ---- verified input transfer (bound input chest -> machine) -------------
// The complete native GimmickProcessPushItem entry is intentionally not
// called: it is hard-wired to CSaveData's selected player slot and 30-slot
// backpack.  The transaction below reproduces only its proven inner object
// operations, sourcing exact item/rank/count fragments from the bound chest.
// Recipe order is the generated data.dat order (recipe_id ascending).  The
// first recipe whose *entire* ingredient set is available wins.
enum class ProductionInputPlanResult : unsigned char {
    Ready,
    NoSatisfiableRecipe,
    DuplicateOwnership,
    Invalid,
};

struct ProductionInputSource {
    void* item;
    size_t chestSlot;
    NearbyItemInfo before;
    int amount;
    bool guardHeld;
    bool applied;
    bool detached;
};

struct ProductionInputIngredient {
    u64 itemId;
    int rank;
    int amount;
    void* clone;
    bool localOwned;
    bool slotPublished;
};

struct ProductionInputPlan {
    void** chestSlots;
    const village_qol::production_automation::ProcessingRecipeTableEntry*
        recipe;
    ProductionInputSource sources[PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT];
    size_t sourceCount;
    ProductionInputIngredient
        ingredients[PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT];
    size_t ingredientCount;
    size_t machineSlot;
    int anchorRank;
    int totalAmount;
    u64 inProcBefore;
    bool silentIntWrites;
    bool silentU64Writes;
};

struct ProductionInputAttemptTrace {
    u64 callbackSequence;
    ProductionDueKind dueKind;
    std::int64_t gameSecond;
    std::int64_t nextRetryGameSecond;
    u64 recipeId;
    std::int64_t candidateSlot;
    u64 candidateItemId;
    int candidateRank;
    int candidateCount;
    const char* plannerResult;
    size_t slotLimit;
    std::int64_t selectedSlot;
    const char* prepareGate;
    const char* reservationGate;
    const char* transactionGate;
    int planned;
};

static void ProductionInitializeInputAttemptTrace(
        ProductionInputAttemptTrace* trace, ProductionDueKind dueKind,
        std::int64_t gameSecond, u64 callbackSequence) {
    if (!trace) return;
    *trace = {};
    trace->callbackSequence = callbackSequence;
    trace->dueKind = dueKind;
    trace->gameSecond = gameSecond;
    trace->nextRetryGameSecond = INT64_MAX;
    trace->candidateSlot = -1;
    trace->candidateRank = -1;
    trace->selectedSlot = -1;
    trace->plannerResult = "not_reached";
    trace->prepareGate = "not_reached";
    trace->reservationGate = "not_attempted";
    trace->transactionGate = "not_started";
}

static void ProductionRetainInputQuarantine(void* item) {
    if (!item) return;
    for (size_t index = 0; index < g_productionInputQuarantineCount; ++index) {
        if (g_productionInputQuarantine[index] == item) return;
    }
    if (g_productionInputQuarantineCount <
            PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT) {
        g_productionInputQuarantine[g_productionInputQuarantineCount++] =
            item;
    }
    void* expected = nullptr;
    g_productionTransferQuarantine.compare_exchange_strong(expected, item);
}

static bool ProductionRecipeSupportsMachine(
        const village_qol::production_automation::ProcessingRecipeTableEntry&
            recipe,
        u64 machineId) {
    for (size_t index = 0; index < recipe.machineCount && index < 2; ++index) {
        if (recipe.machineIds[index] == machineId) return true;
    }
    return false;
}

static size_t ProductionBuildRankOrder(int anchorRank, int* ranks,
                                       size_t capacity) {
    if (!ranks || capacity < 5 || anchorRank < 0 || anchorRank > 4) return 0;
    size_t count = 0;
    for (int rank = anchorRank; rank >= 0; --rank) ranks[count++] = rank;
    for (int rank = anchorRank + 1; rank <= 4; ++rank) ranks[count++] = rank;
    return count;
}

static ProductionInputPlanResult ProductionBuildInputPlan(
        void* inputChestStatus, u64 machineId, ProductionInputPlan* plan,
        ProductionInputAttemptTrace* trace) {
    if (!inputChestStatus || !machineId || !plan) {
        if (trace) trace->plannerResult = "invalid_arguments";
        return ProductionInputPlanResult::Invalid;
    }
    *plan = {};
    NearbyRawPointerVector inventory = {};
    if (!NearbyReadRawInventoryRelaxed(inputChestStatus, &inventory)) {
        if (trace) trace->plannerResult = "input_snapshot_invalid";
        return ProductionInputPlanResult::Invalid;
    }
    const size_t effectiveSlots = (reinterpret_cast<uintptr_t>(inventory.end) -
        reinterpret_cast<uintptr_t>(inventory.begin)) / sizeof(void*);
    const size_t slotCount = effectiveSlots <= PRODUCTION_CHEST_SLOTS
        ? effectiveSlots : PRODUCTION_CHEST_SLOTS;
    NearbyItemInfo itemInfo[PRODUCTION_CHEST_SLOTS] = {};
    bool occupied[PRODUCTION_CHEST_SLOTS] = {};
    void* uniqueItems[PRODUCTION_CHEST_SLOTS] = {};
    size_t uniqueCount = 0;
    for (size_t slot = 0; slot < slotCount; ++slot) {
        void* item = inventory.begin[slot];
        if (!item) continue;
        for (size_t seen = 0; seen < uniqueCount; ++seen) {
            if (uniqueItems[seen] == item) {
                if (trace) trace->plannerResult = "duplicate_ownership";
                return ProductionInputPlanResult::DuplicateOwnership;
            }
        }
        uniqueItems[uniqueCount++] = item;
        if (!NearbyReadItem(item, &itemInfo[slot])) {
            if (trace) trace->plannerResult = "item_read_invalid";
            return ProductionInputPlanResult::Invalid;
        }
        occupied[slot] = true;
    }

    // Convenience policy: chest order is authoritative.  Find the lowest
    // occupied slot whose item is accepted by this exact machine, then test
    // that item's recipes in the generated (recipe-id ascending) order.  A
    // recipe still wins only when its complete multi-item requirement can be
    // satisfied; no partial removal is ever planned.
    const auto* idx = village_qol::production_automation::
        ProductionRecipeMachineIndexLookup(machineId);
    const auto& tbl =
        village_qol::production_automation::kProcessingRecipeTable;
    if (!idx) {
        if (trace) trace->plannerResult = "no_recipes_for_machine";
        return ProductionInputPlanResult::NoSatisfiableRecipe;
    }
    for (size_t anchorSlot = 0; anchorSlot < slotCount;
         ++anchorSlot) {
        if (!occupied[anchorSlot] || itemInfo[anchorSlot].rank < 0 ||
            itemInfo[anchorSlot].rank > 4) continue;
        const u64 anchorItemId = itemInfo[anchorSlot].itemId;
        const int anchorRank = itemInfo[anchorSlot].rank;
        bool acceptedAnchor = false;
        for (u32 ri = 0; ri < idx->recipeCount; ++ri) {
            const auto& recipe = tbl[idx->recipeIndices[ri]];
            if (recipe.inputItemIds[0] != anchorItemId) continue;
            if (!acceptedAnchor) {
                acceptedAnchor = true;
                if (trace) {
                    trace->candidateSlot =
                        static_cast<std::int64_t>(anchorSlot);
                    trace->candidateItemId = anchorItemId;
                    trace->candidateRank = anchorRank;
                    trace->candidateCount = itemInfo[anchorSlot].stackCount;
                    trace->recipeId = recipe.recipeId;
                }
            }
            if (recipe.inputCounts[0] == 0 || recipe.outputItemId == 0 ||
                recipe.outputCount == 0 || recipe.outputCount > 999 ||
                recipe.duration == 0 ||
                recipe.duration > UINT64_MAX / 60ULL) continue;

        ProductionInputPlan candidate = {};
        candidate.chestSlots = inventory.begin;
        candidate.recipe = &recipe;
            candidate.machineSlot = PRODUCTION_MACHINE_SLOTS;
        candidate.anchorRank = anchorRank;
        int available[PRODUCTION_CHEST_SLOTS] = {};
        for (size_t slot = 0; slot < slotCount; ++slot)
            available[slot] = occupied[slot] ? itemInfo[slot].stackCount : 0;
        bool complete = true;
        for (size_t inputIndex = 0; inputIndex < 2; ++inputIndex) {
            const u64 inputId = recipe.inputItemIds[inputIndex];
            int remaining = static_cast<int>(recipe.inputCounts[inputIndex]);
            if (!inputId || remaining == 0) continue;
            if (remaining < 0 || remaining >
                    static_cast<int>(PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT)) {
                complete = false;
                break;
            }
            int rankOrder[5] = {};
            const size_t rankCount = ProductionBuildRankOrder(
                anchorRank, rankOrder, sizeof(rankOrder) / sizeof(rankOrder[0]));
            for (size_t order = 0; order < rankCount && remaining > 0;
                 ++order) {
                const int rank = rankOrder[order];
                const size_t firstSource = candidate.sourceCount;
                int groupAmount = 0;
                for (size_t slot = 0; slot < slotCount &&
                                      remaining > 0; ++slot) {
                    if (!occupied[slot] || available[slot] <= 0 ||
                        itemInfo[slot].itemId != inputId ||
                        itemInfo[slot].rank != rank) continue;
                    const int amount = available[slot] < remaining
                        ? available[slot] : remaining;
                    if (candidate.sourceCount >=
                        PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT) {
                        complete = false;
                        break;
                    }
                    ProductionInputSource& source =
                        candidate.sources[candidate.sourceCount++];
                    source.item = inventory.begin[slot];
                    source.chestSlot = slot;
                    source.before = itemInfo[slot];
                    source.amount = amount;
                    available[slot] -= amount;
                    remaining -= amount;
                    groupAmount += amount;
                }
                if (!complete) break;
                if (groupAmount > 0) {
                    if (candidate.ingredientCount >=
                        PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT) {
                        complete = false;
                        break;
                    }
                    ProductionInputIngredient& ingredient =
                        candidate.ingredients[candidate.ingredientCount++];
                    ingredient.itemId = inputId;
                    ingredient.rank = rank;
                    ingredient.amount = groupAmount;
                    candidate.totalAmount += groupAmount;
                } else if (candidate.sourceCount != firstSource) {
                    complete = false;
                    break;
                }
            }
            if (!complete || remaining != 0) {
                complete = false;
                break;
            }
        }
        if (!complete || candidate.totalAmount <= 0 ||
            candidate.sourceCount == 0 || candidate.ingredientCount == 0 ||
            candidate.ingredientCount >
                PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT) continue;
                *plan = candidate;
                if (trace) {
                    trace->recipeId = recipe.recipeId;
                    trace->plannerResult = "ready";
                    trace->planned = candidate.totalAmount;
                }
                return ProductionInputPlanResult::Ready;
            }
        // Chest order is authoritative: once the first machine-accepted input
        // item is found, recipes for later chest slots are not considered.
        if (acceptedAnchor) {
            if (trace) trace->plannerResult = "no_satisfiable_recipe";
            return ProductionInputPlanResult::NoSatisfiableRecipe;
        }
    }
    if (trace) trace->plannerResult = "no_accepted_input";
    if (trace) {
        const size_t dumpSlots = slotCount < 10 ? slotCount : 10;
        for (size_t slot = 0; slot < dumpSlots; ++slot) {
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=input_inventory_diag "
                "machine_id=%llu slot=%zu occupied=%d item_id=%llu "
                "rank=%d count=%d\n",
                static_cast<unsigned long long>(machineId), slot,
                occupied[slot] ? 1 : 0,
                static_cast<unsigned long long>(itemInfo[slot].itemId),
                itemInfo[slot].rank, itemInfo[slot].stackCount);
        }
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=input_inventory_diag "
            "machine_id=%llu slot_count=%zu raw_slots=%zu "
            "has_machine_recipes=%d\n",
            static_cast<unsigned long long>(machineId), slotCount,
            effectiveSlots,
            village_qol::production_automation::
                    ProductionRecipeMachineIndexLookup(machineId)
                ? 1
                : 0);
    }
    return ProductionInputPlanResult::NoSatisfiableRecipe;
}

static bool ProductionInputSourcesCurrent(
        void* inputChestStatus, const ProductionInputPlan& plan) {
    NearbyRawPointerVector inventory = {};
    if (!inputChestStatus || !plan.chestSlots ||
        !NearbyReadRawInventoryRelaxed(inputChestStatus, &inventory) ||
        inventory.begin != plan.chestSlots) return false;
    const size_t effectiveSlots = (reinterpret_cast<uintptr_t>(inventory.end) -
        reinterpret_cast<uintptr_t>(inventory.begin)) / sizeof(void*);
    const size_t slotCount = effectiveSlots <= PRODUCTION_CHEST_SLOTS
        ? effectiveSlots : PRODUCTION_CHEST_SLOTS;
    void* uniqueItems[PRODUCTION_CHEST_SLOTS] = {};
    size_t uniqueCount = 0;
    for (size_t slot = 0; slot < slotCount; ++slot) {
        void* item = inventory.begin[slot];
        if (!item) continue;
        for (size_t seen = 0; seen < uniqueCount; ++seen) {
            if (uniqueItems[seen] == item) return false;
        }
        uniqueItems[uniqueCount++] = item;
    }
    for (size_t index = 0; index < plan.sourceCount; ++index) {
        const ProductionInputSource& source = plan.sources[index];
        if (source.chestSlot >= PRODUCTION_CHEST_SLOTS ||
            inventory.begin[source.chestSlot] != source.item) return false;
        NearbyItemInfo current = {};
        if (!NearbyReadItem(source.item, &current) ||
            current.itemId != source.before.itemId ||
            current.rank != source.before.rank ||
            current.stackCount != source.before.stackCount ||
            source.amount <= 0 || source.amount > current.stackCount)
            return false;
    }
    return true;
}

static bool ProductionPrepareIdleMachineForInput(
        void* machineStatus, const ProductionBinding& binding,
        ProductionInputPlan* plan, void*** backingOut, void** dataOut,
        std::int64_t gameSecond, ProductionInputAttemptTrace* trace) {
    const auto reject = [&](const char* gate) {
        if (trace) trace->prepareGate = gate;
        return false;
    };
    if (!machineStatus || !plan || !backingOut || !dataOut || gameSecond < 0)
        return reject("invalid_arguments");
    if (!ProductionValidateMachineStatusForNativeTransaction(
            machineStatus, &binding.machine))
        return reject("live_component_invalid");
    const bool intNotifyReady = ProductionCanSetStatusInt(machineStatus);
    const bool u64NotifyReady = ProductionCanSetStatusU64(machineStatus);
    plan->silentIntWrites = !intNotifyReady &&
        ProductionCanSetStatusIntSilent(machineStatus);
    plan->silentU64Writes = !u64NotifyReady &&
        ProductionCanSetStatusU64Silent(machineStatus);
    if (!intNotifyReady && !plan->silentIntWrites)
        return reject("status_int_setter_invalid");
    if (!u64NotifyReady && !plan->silentU64Writes)
        return reject("status_u64_setter_invalid");
    ProductionStableId identity = {};
    if (!ProductionReadGimmickIdentity(machineStatus, &identity))
        return reject("machine_identity_unreadable");
    if (!ProductionIdEqual(identity, binding.machine))
        return reject("machine_identity_changed");
    size_t slotLimit = 0;
    void* data = nullptr;
    if (!ProductionReadMachineSlotLimit(machineStatus, &slotLimit, &data))
        return reject("slot_limit_unreadable");
    if (trace) trace->slotLimit = slotLimit;
    if (slotLimit == 0 || slotLimit > PRODUCTION_MACHINE_SLOTS)
        return reject("slot_limit_out_of_range");

    // inProc is the machine-wide GimmickGetInProcess value: an unfinished
    // sibling timer legitimately keeps it set while another slot is being
    // filled.  Only a value that disagrees with the sibling timers rejects.
    u64 inProc = 0;
    if (!ProductionReadStatusMapValue(machineStatus, 0x18, "inProc",
                                      &inProc, true))
        return reject("inproc_unreadable");
    plan->inProcBefore = inProc;
    const unsigned char* machineBytes =
        reinterpret_cast<const unsigned char*>(machineStatus);
    const auto slotRunning = [&](size_t slot) {
        if (!NearbyIsReadable(
                machineBytes + PRODUCTION_TIME_VALUE_OFFSET +
                    slot * sizeof(u64),
                sizeof(u64)) ||
            !NearbyIsReadable(
                machineBytes + PRODUCTION_CREATE_TIME_OFFSET +
                    slot * sizeof(u64),
                sizeof(u64))) return false;
        const u64 start = *reinterpret_cast<const u64*>(
            machineBytes + PRODUCTION_TIME_VALUE_OFFSET + slot * sizeof(u64));
        const u64 duration = *reinterpret_cast<const u64*>(
            machineBytes + PRODUCTION_CREATE_TIME_OFFSET +
            slot * sizeof(u64));
        return start != 0 && duration != 0 &&
               start <= static_cast<u64>(INT64_MAX) &&
               duration <= static_cast<u64>(INT64_MAX) &&
               gameSecond >= static_cast<std::int64_t>(start) &&
               gameSecond - static_cast<std::int64_t>(start) <
                   static_cast<std::int64_t>(duration);
    };

    // Select the lowest available slot.  Each slot owns its eight backing
    // entries; dormant slots beyond slotLimit are neither read nor required.
    void** backing = nullptr;
    plan->machineSlot = PRODUCTION_MACHINE_SLOTS;
    if (trace) trace->selectedSlot = -1;
    for (size_t slot = 0; slot < slotLimit; ++slot) {
        if (!NearbyIsReadable(
                machineBytes + PRODUCTION_TIME_VALUE_OFFSET +
                    slot * sizeof(u64),
                sizeof(u64)) ||
            !NearbyIsReadable(
                machineBytes + PRODUCTION_CREATE_TIME_OFFSET +
                    slot * sizeof(u64),
                sizeof(u64))) return reject("timer_unreadable");
        if (*reinterpret_cast<const u64*>(
                machineBytes + PRODUCTION_TIME_VALUE_OFFSET +
                slot * sizeof(u64)) != 0 ||
            *reinterpret_cast<const u64*>(
                machineBytes + PRODUCTION_CREATE_TIME_OFFSET +
                slot * sizeof(u64)) != 0) continue;
        char idKey[8] = {};
        char countKey[9] = {};
        _snprintf_s(idKey, sizeof(idKey), _TRUNCATE, "itemID%zu", slot);
        _snprintf_s(countKey, sizeof(countKey), _TRUNCATE, "itemVal%zu", slot);
        u64 itemId = 0;
        u64 itemCount = 0;
        if (!ProductionReadStatusMapValue(machineStatus, 0x58, idKey,
                                          &itemId, false))
            return reject("item_id_map_unreadable");
        if (!ProductionReadStatusMapValue(machineStatus, 0x18, countKey,
                                           &itemCount, true))
            return reject("item_count_map_unreadable");
        if (itemId != 0 || itemCount != 0) continue;
        const size_t requiredBackingEntries =
            (slot + 1) * PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT;
        void** candidateBacking = nullptr;
        if (!ProductionReadMachineItemBacking(
                machineStatus, requiredBackingEntries, &candidateBacking))
            return reject("selected_backing_unreadable");
        const size_t selectedFirst =
            slot * PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT;
        const size_t selectedEnd = selectedFirst +
            PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT;
        bool backingEmpty = true;
        for (size_t index = selectedFirst; index < selectedEnd; ++index) {
            if (candidateBacking[index] != nullptr) {
                backingEmpty = false;
                break;
            }
        }
        if (!backingEmpty) continue;
        plan->machineSlot = slot;
        if (trace) trace->selectedSlot = static_cast<std::int64_t>(slot);
        backing = candidateBacking;
        break;
    }
    if (plan->machineSlot >= PRODUCTION_MACHINE_SLOTS ||
        plan->machineSlot >= slotLimit)
        return reject("no_idle_slot");
    bool siblingRunning = false;
    for (size_t slot = 0; slot < slotLimit; ++slot) {
        if (slot == plan->machineSlot) continue;
        if (!slotRunning(slot)) continue;
        siblingRunning = true;
        break;
    }
    if ((siblingRunning && inProc == 0) || (!siblingRunning && inProc != 0))
        return reject(siblingRunning ? "inproc_should_be_set" :
                                       "inproc_busy");
    *backingOut = backing;
    *dataOut = data;
    if (trace) trace->prepareGate = "ready";
    return true;
}

static bool ProductionInputMachineStorageCurrent(
        void* machineStatus, const ProductionBinding& binding,
        const ProductionInputPlan& plan, void** expectedBacking,
        void* expectedData) {
    if (!machineStatus || !expectedBacking || !expectedData) return false;
    ProductionStableId identity = {};
    size_t slotLimit = 0;
    void* currentData = nullptr;
    void** currentBacking = nullptr;
    return ProductionValidateMachineStatusForNativeTransaction(
               machineStatus, &binding.machine) &&
        ProductionReadGimmickIdentity(machineStatus, &identity) &&
        ProductionIdEqual(identity, binding.machine) &&
        ProductionReadMachineSlotLimit(machineStatus, &slotLimit,
                                       &currentData) &&
        plan.machineSlot < slotLimit && currentData == expectedData &&
        ProductionReadMachineItemBacking(
            machineStatus,
            (plan.machineSlot + 1) *
                PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT,
            &currentBacking) &&
        currentBacking == expectedBacking;
}

static bool ProductionInputMachinePreparedForBacking(
        void* machineStatus, const ProductionBinding& binding,
        const ProductionInputPlan& plan, void** expectedBacking,
        void* expectedData, u64 start, u64 duration,
        std::int64_t gameSecond) {
    if (!plan.recipe || gameSecond < 0 ||
        !ProductionInputMachineStorageCurrent(
            machineStatus, binding, plan, expectedBacking, expectedData))
        return false;
    const unsigned char* bytes =
        reinterpret_cast<const unsigned char*>(machineStatus);
    const size_t slot = plan.machineSlot;
    if (!NearbyIsReadable(
            bytes + PRODUCTION_TIME_VALUE_OFFSET + slot * sizeof(u64),
            sizeof(u64)) ||
        !NearbyIsReadable(
            bytes + PRODUCTION_CREATE_TIME_OFFSET + slot * sizeof(u64),
            sizeof(u64)) ||
        *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_TIME_VALUE_OFFSET + slot * sizeof(u64)) !=
            start ||
        *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_CREATE_TIME_OFFSET + slot * sizeof(u64)) !=
            duration) return false;
    char idKey[8] = {};
    char countKey[9] = {};
    _snprintf_s(idKey, sizeof(idKey), _TRUNCATE, "itemID%zu", slot);
    _snprintf_s(countKey, sizeof(countKey), _TRUNCATE, "itemVal%zu", slot);
    u64 itemId = 0;
    u64 itemCount = 0;
    u64 inProc = 0;
    if (!ProductionReadStatusMapValue(machineStatus, 0x58, idKey,
                                      &itemId, false) ||
        !ProductionReadStatusMapValue(machineStatus, 0x18, countKey,
                                      &itemCount, true) ||
        !ProductionReadStatusMapValue(machineStatus, 0x18, "inProc",
                                      &inProc, true) ||
        itemId != plan.recipe->outputItemId ||
        itemCount != plan.recipe->outputCount) return false;
    // GimmickGetInProcess covers any unfinished timer on this machine.  The
    // selected slot is now running, so inProc may already be 1 from a sibling;
    // a zero value is valid only when no sibling timer is unfinished.
    size_t slotLimit = 0;
    if (!ProductionReadMachineSlotLimit(machineStatus, &slotLimit) ||
        slot > slotLimit) return false;
    bool siblingRunning = false;
    for (size_t other = 0; other < slotLimit; ++other) {
        if (other == slot) continue;
        const u64 otherStart = *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_TIME_VALUE_OFFSET + other * sizeof(u64));
        const u64 otherDuration = *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_CREATE_TIME_OFFSET + other * sizeof(u64));
        if (!otherStart || !otherDuration ||
            otherStart > static_cast<u64>(INT64_MAX) ||
            otherDuration > static_cast<u64>(INT64_MAX) ||
            gameSecond < static_cast<std::int64_t>(otherStart) ||
            gameSecond - static_cast<std::int64_t>(otherStart) >=
                static_cast<std::int64_t>(otherDuration)) continue;
        siblingRunning = true;
        break;
    }
    const u64 expectedInProc = siblingRunning ? 1 : 0;
    if (inProc != expectedInProc) return false;
    const size_t firstBacking =
        plan.machineSlot * PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT;
    const size_t endBacking = firstBacking +
        PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT;
    for (size_t index = firstBacking; index < endBacking; ++index) {
        if (expectedBacking[index] != nullptr) return false;
    }
    return true;
}

static bool ProductionCreateInputIngredients(
        void* save, ProductionInputPlan* plan) {
    if (!save || !plan || !plan->recipe ||
        !g_productionItemAllocate || !g_productionItemCtor ||
        !g_productionItemSetCount || !g_nearbyIntrusiveRelease)
        return false;
    for (size_t index = 0; index < plan->ingredientCount; ++index) {
        ProductionInputIngredient& ingredient = plan->ingredients[index];
        void* memory = g_productionItemAllocate(0x2c0);
        if (!memory) return false;
        if (!NearbyIsReadable(memory, 0x2c0)) {
            ProductionRetainInputQuarantine(memory);
            return false;
        }
        // The verified native allocator may already zero memory depending on
        // its runtime flag; PushItem nevertheless zeroes the full block again.
        memset(memory, 0, 0x2c0);
        void* item = g_productionItemCtor(memory, save, ingredient.itemId);
        if (!item || item != memory ||
            !NearbyIsReadable(item, NEARBY_ITEM_RANK_OFFSET + sizeof(int))) {
            // A signature-verified ctor is expected to return the same block.
            // If that invariant is ever violated, neither raw-free nor an
            // intrusive release is provably legal; retain every reachable
            // address and fault the whole transaction instead of leaking an
            // untracked allocation or releasing an unowned object.
            ProductionRetainInputQuarantine(memory);
            if (item && item != memory)
                ProductionRetainInputQuarantine(item);
            return false;
        }
        // ctor starts at refcount zero.  Establish the transaction-local
        // ownership before any later check can abandon the object.
        NearbyAddReference(item);
        ingredient.clone = item;
        ingredient.localOwned = true;
        *reinterpret_cast<int*>(
            reinterpret_cast<unsigned char*>(item) +
            NEARBY_ITEM_RANK_OFFSET) = ingredient.rank;
        g_productionItemSetCount(item, ingredient.amount);
        NearbyItemInfo current = {};
        if (!NearbyReadItem(item, &current) ||
            current.itemId != ingredient.itemId ||
            current.rank != ingredient.rank ||
            current.stackCount != ingredient.amount) return false;
    }
    return true;
}

static bool ProductionReleaseInputIngredientLocals(
        ProductionInputPlan* plan, bool retainPublished) {
    if (!plan) return false;
    bool allReleased = true;
    for (size_t index = plan->ingredientCount; index > 0; --index) {
        ProductionInputIngredient& ingredient = plan->ingredients[index - 1];
        if (!ingredient.localOwned || !ingredient.clone) continue;
        if (retainPublished && ingredient.slotPublished) {
            ProductionRetainInputQuarantine(ingredient.clone);
            allReleased = false;
            continue;
        }
        NearbyReleaseOwnedReference(ingredient.clone);
        ingredient.localOwned = false;
    }
    return allReleased;
}

static void ProductionReleaseInputSourceGuards(
        ProductionInputPlan* plan, bool retain) {
    if (!plan) return;
    for (size_t reverse = plan->sourceCount; reverse > 0; --reverse) {
        ProductionInputSource& source = plan->sources[reverse - 1];
        if (!source.guardHeld || !source.item) continue;
        if (retain) {
            ProductionRetainInputQuarantine(source.item);
            continue;
        }
        NearbyReleaseOwnedReference(source.item);
        source.guardHeld = false;
    }
}

static bool ProductionApplyInputSources(
        const ProductionDueWork& work, const ProductionBinding& binding,
        void* machineStatus, void* inputChestStatus,
        ProductionInputPlan* plan, void** expectedBacking,
        void* expectedData, u64 start, u64 duration,
        ProductionInputAttemptTrace* trace) {
    if (!plan || !plan->chestSlots || !machineStatus || !inputChestStatus ||
        !expectedBacking || !expectedData || !trace || !g_nearbyItemAdjust ||
        !g_nearbyIntrusiveRelease) {
        if (trace) trace->transactionGate = "source_apply_arguments_invalid";
        return false;
    }
    const auto validateAfterHelper = [&](const char* storageGate) {
        const char* gate = ProductionReservedInputTransactionGate(
            work, binding);
        if (strcmp(gate, "reserved") != 0) {
            trace->transactionGate = gate;
            return false;
        }
        if (!ProductionInputMachinePreparedForBacking(
                machineStatus, binding, *plan, expectedBacking,
                expectedData, start, duration, trace->gameSecond)) {
            trace->transactionGate = storageGate;
            return false;
        }
        ProductionStableId inputIdentity = {};
        if (!ProductionReadGimmickIdentity(inputChestStatus,
                                            &inputIdentity)) {
            trace->transactionGate = "input_identity_unreadable_after_helper";
            return false;
        }
        if (!ProductionIdEqual(inputIdentity, binding.input)) {
            trace->transactionGate = "input_identity_changed_after_helper";
            return false;
        }
        return true;
    };
    const auto readCurrentSourceSlot = [&](size_t chestSlot,
                                            void*** slotOut) {
        if (!slotOut || chestSlot >= PRODUCTION_CHEST_SLOTS) return false;
        NearbyRawPointerVector inventory = {};
        if (!NearbyReadRawInventoryRelaxed(inputChestStatus, &inventory) ||
            inventory.begin != plan->chestSlots) return false;
        void** slot = &inventory.begin[chestSlot];
        if (!NearbyIsReadable(slot, sizeof(void*))) return false;
        *slotOut = slot;
        return true;
    };
    for (size_t index = 0; index < plan->sourceCount; ++index) {
        ProductionInputSource& source = plan->sources[index];
        NearbyAddReference(source.item);
        source.guardHeld = true;
    }
    for (size_t index = 0; index < plan->sourceCount; ++index) {
        ProductionInputSource& source = plan->sources[index];
        void** slot = nullptr;
        NearbyItemInfo current = {};
        if (!readCurrentSourceSlot(source.chestSlot, &slot) ||
            *slot != source.item ||
            !NearbyReadItem(source.item, &current) ||
            current.itemId != source.before.itemId ||
            current.rank != source.before.rank ||
            current.stackCount != source.before.stackCount) {
            trace->transactionGate = "source_precommit_changed";
            return false;
        }
        source.applied = true;
        g_nearbyItemAdjust(source.item, -source.amount);
        const int expectedRemaining =
            source.before.stackCount - source.amount;
        int remaining = -1;
        if (!readCurrentSourceSlot(source.chestSlot, &slot) ||
            !NearbyReadStackCount(source.item, &remaining) ||
            remaining != expectedRemaining || *slot != source.item) {
            trace->transactionGate = "source_adjust_readback_failed";
            return false;
        }
        if (!validateAfterHelper("machine_storage_changed_after_source_adjust"))
            return false;
        if (expectedRemaining == 0) {
            void* observed = InterlockedCompareExchangePointer(
                reinterpret_cast<void* volatile*>(slot), nullptr,
                source.item);
            if (observed != source.item) {
                trace->transactionGate = "source_slot_detach_failed";
                return false;
            }
            // Release exactly the chest-slot-owned reference.  The guard
            // remains until machine publication is fully verified.
            NearbyReleaseOwnedReference(source.item);
            source.detached = true;
            if (!readCurrentSourceSlot(source.chestSlot, &slot) ||
                *slot != nullptr ||
                !NearbyReadStackCount(source.item, &remaining) ||
                remaining != 0) {
                trace->transactionGate = "source_detach_readback_failed";
                return false;
            }
            if (!validateAfterHelper(
                    "machine_storage_changed_after_source_release"))
                return false;
        }
    }
    return true;
}

static bool ProductionRollbackInputSources(
        void* inputChestStatus, ProductionInputPlan* plan) {
    NearbyRawPointerVector currentInventory = {};
    if (!inputChestStatus || !plan || !plan->chestSlots ||
        !NearbyReadRawInventoryRelaxed(inputChestStatus, &currentInventory) ||
        currentInventory.begin != plan->chestSlots) {
        ProductionReleaseInputSourceGuards(plan, true);
        return false;
    }
    const auto readRollbackSlot = [&](size_t chestSlot, void*** slotOut) {
        if (!slotOut || chestSlot >= PRODUCTION_CHEST_SLOTS) return false;
        NearbyRawPointerVector inventory = {};
        if (!NearbyReadRawInventoryRelaxed(inputChestStatus, &inventory) ||
            inventory.begin != plan->chestSlots) return false;
        void** slot = &inventory.begin[chestSlot];
        if (!NearbyIsReadable(slot, sizeof(void*))) return false;
        *slotOut = slot;
        return true;
    };
    bool restored = true;
    for (size_t reverse = plan->sourceCount; reverse > 0; --reverse) {
        ProductionInputSource& source = plan->sources[reverse - 1];
        if (!source.applied) continue;
        void** slot = nullptr;
        if (!readRollbackSlot(source.chestSlot, &slot)) {
            restored = false;
            continue;
        }
        int current = -1;
        if (!NearbyReadStackCount(source.item, &current)) {
            restored = false;
            continue;
        }
        if (!source.detached) {
            if (*slot != source.item) {
                restored = false;
                continue;
            }
            if (current != source.before.stackCount)
                g_nearbyItemAdjust(
                    source.item, source.before.stackCount - current);
            if (!readRollbackSlot(source.chestSlot, &slot) ||
                !NearbyReadStackCount(source.item, &current) ||
                current != source.before.stackCount || *slot != source.item)
                restored = false;
            continue;
        }
        if (*slot != nullptr) {
            restored = false;
            continue;
        }
        if (current != source.before.stackCount)
            g_nearbyItemAdjust(source.item,
                               source.before.stackCount - current);
        if (!readRollbackSlot(source.chestSlot, &slot) ||
            !NearbyReadStackCount(source.item, &current) ||
            current != source.before.stackCount || *slot != nullptr) {
            restored = false;
            continue;
        }
        NearbyAddReference(source.item); // restore chest-slot ownership
        void* observed = InterlockedCompareExchangePointer(
            reinterpret_cast<void* volatile*>(slot), source.item, nullptr);
        if (observed != nullptr) {
            NearbyReleaseOwnedReference(source.item);
            restored = false;
            continue;
        }
        if (*slot != source.item) {
            void* removed = InterlockedCompareExchangePointer(
                reinterpret_cast<void* volatile*>(slot), nullptr,
                source.item);
            if (removed == source.item)
                NearbyReleaseOwnedReference(source.item);
            restored = false;
            continue;
        }
        source.detached = false;
    }
    ProductionReleaseInputSourceGuards(plan, !restored);
    return restored;
}

static bool ProductionVerifyInputCommit(
        void* machineStatus, void* inputChestStatus,
        const ProductionBinding& binding, const ProductionInputPlan& plan,
        void** backing, void* expectedData, u64 start, u64 duration) {
    if (!machineStatus || !inputChestStatus || !plan.recipe || !backing ||
        !expectedData) return false;
    ProductionStableId machineIdentity = {};
    ProductionStableId inputIdentity = {};
    size_t slotLimit = 0;
    void* currentData = nullptr;
    void** currentBacking = nullptr;
    if (!ProductionReadGimmickIdentity(machineStatus, &machineIdentity) ||
        !ProductionReadGimmickIdentity(inputChestStatus, &inputIdentity) ||
        !ProductionIdEqual(machineIdentity, binding.machine) ||
        !ProductionIdEqual(inputIdentity, binding.input) ||
        !ProductionReadMachineSlotLimit(machineStatus, &slotLimit,
                                        &currentData) ||
        currentData != expectedData || plan.machineSlot >= slotLimit ||
        !ProductionReadMachineItemBacking(
            machineStatus,
            (plan.machineSlot + 1) *
                PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT,
            &currentBacking) ||
        currentBacking != backing)
        return false;
    const unsigned char* bytes =
        reinterpret_cast<const unsigned char*>(machineStatus);
    if (!NearbyIsReadable(
            bytes + PRODUCTION_TIME_VALUE_OFFSET +
                plan.machineSlot * sizeof(u64),
            sizeof(u64)) ||
        !NearbyIsReadable(
            bytes + PRODUCTION_CREATE_TIME_OFFSET +
                plan.machineSlot * sizeof(u64),
            sizeof(u64)) ||
        *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_TIME_VALUE_OFFSET +
            plan.machineSlot * sizeof(u64)) != start ||
        *reinterpret_cast<const u64*>(
            bytes + PRODUCTION_CREATE_TIME_OFFSET +
            plan.machineSlot * sizeof(u64)) != duration)
        return false;
    char idKey[8] = {};
    char countKey[9] = {};
    _snprintf_s(idKey, sizeof(idKey), _TRUNCATE, "itemID%zu",
                plan.machineSlot);
    _snprintf_s(countKey, sizeof(countKey), _TRUNCATE, "itemVal%zu",
                plan.machineSlot);
    u64 itemId = 0;
    u64 itemCount = 0;
    u64 inProc = 0;
    if (!ProductionReadStatusMapValue(machineStatus, 0x58, idKey,
                                      &itemId, false) ||
        !ProductionReadStatusMapValue(machineStatus, 0x18, countKey,
                                      &itemCount, true) ||
        !ProductionReadStatusMapValue(machineStatus, 0x18, "inProc",
                                      &inProc, true) ||
        itemId != plan.recipe->outputItemId ||
        itemCount != plan.recipe->outputCount || inProc != 1)
        return false;
    const size_t first = plan.machineSlot *
        PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT;
    for (size_t index = 0; index < plan.ingredientCount; ++index) {
        const ProductionInputIngredient& ingredient =
            plan.ingredients[index];
        if (!ingredient.clone || backing[first + index] != ingredient.clone)
            return false;
        NearbyItemInfo current = {};
        if (!NearbyReadItem(ingredient.clone, &current) ||
            current.itemId != ingredient.itemId ||
            current.rank != ingredient.rank ||
            current.stackCount != ingredient.amount) return false;
    }
    for (size_t index = plan.ingredientCount;
         index < PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT; ++index) {
        if (backing[first + index] != nullptr) return false;
    }
    NearbyRawPointerVector inventory = {};
    if (!NearbyReadRawInventoryRelaxed(inputChestStatus, &inventory) ||
        inventory.begin != plan.chestSlots) return false;
    for (size_t index = 0; index < plan.sourceCount; ++index) {
        const ProductionInputSource& source = plan.sources[index];
        const int expected = source.before.stackCount - source.amount;
        int current = -1;
        if (!NearbyReadStackCount(source.item, &current) ||
            current != expected) return false;
        if ((expected == 0 && inventory.begin[source.chestSlot] != nullptr) ||
            (expected != 0 &&
             inventory.begin[source.chestSlot] != source.item)) return false;
    }
    return true;
}

struct ProductionInputFailureLogThrottle {
    ProductionStableId machine;
    std::int64_t lastGameSecond;
    const char* reason;
    bool used;
};

static ProductionInputFailureLogThrottle
    g_productionInputFailureLogThrottle[PRODUCTION_MAX_BINDINGS] = {};

static bool ProductionShouldLogInputFailure(
        const ProductionBinding& binding, const char* reason,
        std::int64_t gameSecond) {
    ProductionInputFailureLogThrottle* freeEntry = nullptr;
    for (size_t index = 0; index < PRODUCTION_MAX_BINDINGS; ++index) {
        ProductionInputFailureLogThrottle& entry =
            g_productionInputFailureLogThrottle[index];
        if (!entry.used) {
            if (!freeEntry) freeEntry = &entry;
            continue;
        }
        if (!ProductionIdEqual(entry.machine, binding.machine)) continue;
        const bool sameReason = entry.reason == reason ||
            (entry.reason && reason && strcmp(entry.reason, reason) == 0);
        if (sameReason && gameSecond >= entry.lastGameSecond &&
            gameSecond - entry.lastGameSecond <
                PRODUCTION_INPUT_RETRY_GAME_TICKS) return false;
        entry.lastGameSecond = gameSecond;
        entry.reason = reason;
        return true;
    }
    if (!freeEntry) freeEntry = &g_productionInputFailureLogThrottle[0];
    freeEntry->machine = binding.machine;
    freeEntry->lastGameSecond = gameSecond;
    freeEntry->reason = reason;
    freeEntry->used = true;
    return true;
}

struct ProductionOutputFailureLogThrottle {
    ProductionStableId machine;
    std::int64_t lastGameSecond;
    const char* reason;
    bool used;
};

static ProductionOutputFailureLogThrottle
    g_productionOutputFailureLogThrottle[PRODUCTION_MAX_BINDINGS] = {};

static bool ProductionShouldLogOutputFailure(
        const ProductionBinding& binding, const char* reason,
        std::int64_t gameSecond) {
    ProductionOutputFailureLogThrottle* freeEntry = nullptr;
    for (size_t index = 0; index < PRODUCTION_MAX_BINDINGS; ++index) {
        ProductionOutputFailureLogThrottle& entry =
            g_productionOutputFailureLogThrottle[index];
        if (!entry.used) {
            if (!freeEntry) freeEntry = &entry;
            continue;
        }
        if (!ProductionIdEqual(entry.machine, binding.machine)) continue;
        const bool sameReason = entry.reason == reason ||
            (entry.reason && reason && strcmp(entry.reason, reason) == 0);
        if (sameReason && gameSecond >= entry.lastGameSecond &&
            gameSecond - entry.lastGameSecond <
                PRODUCTION_OUTPUT_RETRY_GAME_TICKS) return false;
        entry.lastGameSecond = gameSecond;
        entry.reason = reason;
        return true;
    }
    if (!freeEntry) freeEntry = &g_productionOutputFailureLogThrottle[0];
    freeEntry->machine = binding.machine;
    freeEntry->lastGameSecond = gameSecond;
    freeEntry->reason = reason;
    freeEntry->used = true;
    return true;
}

// Every no-write output rejection must leave one explicit, throttled record
// carrying the exact machine/output identity, slot and rejection gate so a
// live run can never end with a mature binding silently unresolved.
static void ProductionLogOutputNoWrite(
        u64 callbackSequence, const ProductionBinding& binding,
        const ProductionObjectSnapshot* machine, size_t slot,
        u64 itemId, int rank, int count, ProductionDueKind dueKind,
        const char* gate, const char* reason, std::int64_t gameSecond,
        std::int64_t nextRetryGameSecond, ProductionState stateAfter) {
    if (!ProductionShouldLogOutputFailure(binding, reason, gameSecond))
        return;
    const u64 txn = g_productionTxnSequence.fetch_add(
        1, std::memory_order_relaxed) + 1;
    char inputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    char outputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    strcpy_s(inputName, binding.hasInput ? "unresolved" : "unbound");
    strcpy_s(outputName, binding.hasOutput ? "unresolved" : "unbound");
    AcquireSRWLockShared(&g_productionPublishLock);
    for (size_t index = 0; index < g_productionPublished.chestCount; ++index) {
        const ProductionObjectSnapshot& chest =
            g_productionPublished.chests[index];
        if (binding.hasInput && ProductionIdEqual(binding.input, chest.id))
            ProductionCopyLogToken(chest.name, inputName, sizeof(inputName));
        if (binding.hasOutput && ProductionIdEqual(binding.output, chest.id))
            ProductionCopyLogToken(chest.name, outputName, sizeof(outputName));
    }
    ReleaseSRWLockShared(&g_productionPublishLock);
    const u64 recipeId = machine && slot < machine->slotLimit
        ? ProductionInferRecipeId(machine, slot) : 0;
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=%llu event=transfer action=output "
        "callback_sequence=%llu "
        "device_map_id=%llu device_id=%llu input_map_id=%llu input_id=%llu "
        "output_map_id=%llu output_id=%llu input_name=%s output_name=%s "
        "due_kind=%s game_second=%lld machine_type=%llu module=%s "
        "slot=%zu slot_limit=%zu recipe=%llu output_item=%llu rank=%d "
        "count=%d gate=%s next_retry_game_second=%lld "
        "removed=0 written=0 rollback=not_needed state=%s result=INCOMPLETE "
        "reason=%s case=output_no_write raw_pointer_cache=0 writes=0\n",
        static_cast<unsigned long long>(txn),
        static_cast<unsigned long long>(callbackSequence),
        static_cast<unsigned long long>(binding.machine.mapId),
        static_cast<unsigned long long>(binding.machine.uniqueId),
        static_cast<unsigned long long>(binding.input.mapId),
        static_cast<unsigned long long>(binding.input.uniqueId),
        static_cast<unsigned long long>(binding.output.mapId),
        static_cast<unsigned long long>(binding.output.uniqueId),
        inputName, outputName, ProductionDueKindName(dueKind),
        static_cast<long long>(gameSecond),
        static_cast<unsigned long long>(machine ? machine->gimmickId : 0),
        machine ? machine->module : "unresolved",
        slot, machine ? machine->slotLimit : 0,
        static_cast<unsigned long long>(recipeId),
        static_cast<unsigned long long>(itemId), rank, count,
        gate ? gate : "unknown",
        static_cast<long long>(nextRetryGameSecond),
        ProductionStateName(stateAfter), reason ? reason : "unknown");
}

static void ProductionLogInputTransfer(
        u64 txn, const ProductionBinding& binding,
        const ProductionInputAttemptTrace& trace, const char* outcome,
        const char* reason, const char* rollback, ProductionState stateAfter,
        int removed, int written, int started, int writes) {
    char inputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    char outputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
    strcpy_s(inputName, binding.hasInput ? "unresolved" : "unbound");
    strcpy_s(outputName, binding.hasOutput ? "unresolved" : "unbound");
    AcquireSRWLockShared(&g_productionPublishLock);
    for (size_t index = 0; index < g_productionPublished.chestCount; ++index) {
        const ProductionObjectSnapshot& chest =
            g_productionPublished.chests[index];
        if (binding.hasInput && ProductionIdEqual(binding.input, chest.id))
            ProductionCopyLogToken(chest.name, inputName, sizeof(inputName));
        if (binding.hasOutput && ProductionIdEqual(binding.output, chest.id))
            ProductionCopyLogToken(chest.name, outputName,
                                   sizeof(outputName));
    }
    ReleaseSRWLockShared(&g_productionPublishLock);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=%llu event=transfer action=input "
        "callback_sequence=%llu "
        "device_map_id=%llu device_id=%llu input_map_id=%llu input_id=%llu "
        "output_map_id=%llu output_id=%llu input_name=%s output_name=%s "
        "due_kind=%s game_second=%lld recipe=%llu "
        "candidate_slot=%lld candidate_item_id=%llu candidate_rank=%d "
        "candidate_count=%d planner=%s slot_limit=%zu selected_slot=%lld "
        "prepare_gate=%s reservation_gate=%s transaction_gate=%s "
        "planned=%d removed=%d written=%d started=%d "
        "next_retry_game_second=%lld rollback=%s "
        "state=%s from=%s to=%s result=%s reason=%s "
        "case=first_satisfiable_recipe raw_pointer_cache=0 writes=%d\n",
        static_cast<unsigned long long>(txn),
        static_cast<unsigned long long>(trace.callbackSequence),
        static_cast<unsigned long long>(binding.machine.mapId),
        static_cast<unsigned long long>(binding.machine.uniqueId),
        static_cast<unsigned long long>(binding.input.mapId),
        static_cast<unsigned long long>(binding.input.uniqueId),
        static_cast<unsigned long long>(binding.output.mapId),
        static_cast<unsigned long long>(binding.output.uniqueId),
        inputName, outputName,
        ProductionDueKindName(trace.dueKind),
        static_cast<long long>(trace.gameSecond),
        static_cast<unsigned long long>(trace.recipeId),
        static_cast<long long>(trace.candidateSlot),
        static_cast<unsigned long long>(trace.candidateItemId),
        trace.candidateRank, trace.candidateCount,
        trace.plannerResult ? trace.plannerResult : "not_reached",
        trace.slotLimit, static_cast<long long>(trace.selectedSlot),
        trace.prepareGate ? trace.prepareGate : "not_reached",
        trace.reservationGate ? trace.reservationGate : "not_attempted",
        trace.transactionGate ? trace.transactionGate : "not_started",
        trace.planned, removed, written, started,
        static_cast<long long>(trace.nextRetryGameSecond), rollback,
        ProductionStateName(stateAfter),
        ProductionStateName(binding.state),
        ProductionStateName(stateAfter), outcome, reason, writes);
}

static void ProductionLogInputNoWrite(
        const ProductionBinding& binding,
        const ProductionInputAttemptTrace& trace, const char* reason,
        ProductionState stateAfter) {
    if (!ProductionShouldLogInputFailure(binding, reason, trace.gameSecond))
        return;
    ProductionLogInputTransfer(
        0, binding, trace, "INCOMPLETE", reason, "not_needed", stateAfter,
        0, 0, 0, 0);
}

enum class ProductionInputAttemptResult : unsigned char {
    Started,
    NoSatisfiableRecipe,
    MachineNotIdle,
    ResolveInvalid,
    Faulted,
};

static ProductionInputAttemptResult ProductionAttemptInputTransfer(
        void* save, void* machineStatus, void* inputChestStatus,
        const ProductionDueWork& work, const ProductionBinding& binding,
        const ProductionObjectSnapshot* machine, std::int64_t gameSecond,
        bool* started, bool* mutationAttempted,
        std::int64_t* expectedFinishGameSecond,
        ProductionInputAttemptTrace* trace) {
    if (started) *started = false;
    if (mutationAttempted) *mutationAttempted = false;
    if (expectedFinishGameSecond) *expectedFinishGameSecond = 0;
    const auto reject = [&](ProductionInputAttemptResult result,
                            const char* gate) {
        if (trace) trace->transactionGate = gate;
        return result;
    };
    if (!trace) return ProductionInputAttemptResult::ResolveInvalid;
    if (!save) return reject(ProductionInputAttemptResult::ResolveInvalid,
                             "save_invalid");
    if (!machineStatus) return reject(
        ProductionInputAttemptResult::ResolveInvalid,
        "machine_status_unresolved");
    if (!inputChestStatus) return reject(
        ProductionInputAttemptResult::ResolveInvalid,
        "input_status_unresolved");
    if (!machine) return reject(ProductionInputAttemptResult::ResolveInvalid,
                                "machine_snapshot_missing");
    if (!binding.hasInput || !binding.enabled || !binding.committed ||
        !(binding.state == ProductionState::WaitingInput ||
          (binding.state == ProductionState::Running &&
           binding.dueKind == ProductionDueKind::InputRetry)))
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "binding_state_invalid");
    if (gameSecond <= 0)
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "game_second_invalid");
    if (!g_productionItemAllocate || !g_productionItemCtor ||
        !g_productionItemSetCount || !g_nearbyItemAdjust ||
        !g_nearbyIntrusiveRelease || !g_productionStatusIntLookup ||
        !g_productionStatusU64Lookup || !g_productionStatusEventLookup ||
        !g_productionStatusEventNotify)
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "native_helpers_unavailable");
    if (g_productionTransferFaulted.load(std::memory_order_acquire))
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "transfer_faulted");

    trace->transactionGate = ProductionReservedInputTransactionGate(
        work, binding);
    if (strcmp(trace->transactionGate, "reserved") != 0)
        return ProductionInputAttemptResult::ResolveInvalid;

    ProductionStableId machineIdentity = {};
    ProductionStableId inputIdentity = {};
    if (!ProductionValidateMachineStatusForNativeTransaction(
            machineStatus, &binding.machine))
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "machine_component_invalid");
    if (!ProductionReadGimmickIdentity(machineStatus, &machineIdentity))
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "machine_identity_unreadable");
    if (!ProductionReadGimmickIdentity(inputChestStatus, &inputIdentity))
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "input_identity_unreadable");
    if (!ProductionIdEqual(machineIdentity, binding.machine))
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "machine_identity_changed");
    if (!ProductionIdEqual(inputIdentity, binding.input))
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "input_identity_changed");
    if (machine->gimmickId == 240250000ULL) {
        trace->plannerResult = "machine_recipe_unsupported";
        return reject(ProductionInputAttemptResult::NoSatisfiableRecipe,
                      "planner_rejected");
    }

    ProductionInputPlan plan = {};
    const ProductionInputPlanResult planResult = ProductionBuildInputPlan(
        inputChestStatus, machine->gimmickId, &plan, trace);
    if (planResult == ProductionInputPlanResult::NoSatisfiableRecipe)
        return reject(ProductionInputAttemptResult::NoSatisfiableRecipe,
                      "planner_rejected");
    if (planResult != ProductionInputPlanResult::Ready) {
        const char* reason =
            planResult == ProductionInputPlanResult::DuplicateOwnership
                ? "input_inventory_alias"
                : "input_inventory_invalid";
        const u64 txn = g_productionTxnSequence.fetch_add(
            1, std::memory_order_relaxed) + 1;
        ProductionFaultTransfer(reason, nullptr);
        trace->transactionGate = reason;
        trace->nextRetryGameSecond = INT64_MAX;
        ProductionLogInputTransfer(
            txn, binding, *trace, "FAIL", reason, "not_needed",
            ProductionState::Faulted, 0, 0, 0, 0);
        return ProductionInputAttemptResult::Faulted;
    }

    void** backing = nullptr;
    void* machineData = nullptr;
    if (!ProductionPrepareIdleMachineForInput(
            machineStatus, binding, &plan, &backing, &machineData,
            gameSecond, trace))
        return reject(ProductionInputAttemptResult::MachineNotIdle,
                      "machine_prepare_rejected");
    if (!ProductionInputSourcesCurrent(inputChestStatus, plan))
        return reject(ProductionInputAttemptResult::ResolveInvalid,
                      "source_current_preclone_failed");
    trace->transactionGate = "preclone_validated";

    if (mutationAttempted) *mutationAttempted = true;
    g_productionPreUpdateStage.store(8, std::memory_order_release);
    if (!ProductionCreateInputIngredients(save, &plan)) {
        const bool released = ProductionReleaseInputIngredientLocals(
            &plan, false);
        const u64 txn = g_productionTxnSequence.fetch_add(
            1, std::memory_order_relaxed) + 1;
        if (!released) {
            for (size_t index = 0; index < plan.ingredientCount; ++index) {
                if (plan.ingredients[index].localOwned)
                    ProductionRetainInputQuarantine(
                        plan.ingredients[index].clone);
            }
        }
        ProductionFaultTransfer("input_clone_create_failed",
                                g_productionTransferQuarantine.load());
        trace->transactionGate = "input_clone_create_failed";
        trace->nextRetryGameSecond = INT64_MAX;
        ProductionLogInputTransfer(
            txn, binding, *trace, "FAIL", "input_clone_create_failed",
            released ? "verified" : "failed", ProductionState::Faulted,
            0, 0, 0, 1);
        return ProductionInputAttemptResult::Faulted;
    }

    // Allocation/ctor registers CItemStatus objects and may advance the save's
    // unique-ID counter.  Revalidate every live identity and value plan after
    // those native calls and before the first inventory/machine write.  A
    // clean abort destroys/unregisters the clones; the monotonic ID gap is
    // intentionally not rewound.
    void** backingAgain = nullptr;
    void* machineDataAgain = nullptr;
    ProductionStableId machineIdentityAgain = {};
    ProductionStableId inputIdentityAgain = {};
    const auto abortPostClone = [&](const char* gate) {
        trace->transactionGate = gate;
        if (ProductionReleaseInputIngredientLocals(&plan, false))
            return ProductionInputAttemptResult::ResolveInvalid;
        trace->nextRetryGameSecond = INT64_MAX;
        const u64 txn = g_productionTxnSequence.fetch_add(
            1, std::memory_order_relaxed) + 1;
        ProductionFaultTransfer("input_clone_release_failed",
                                g_productionTransferQuarantine.load());
        ProductionLogInputTransfer(
            txn, binding, *trace, "FAIL", "input_clone_release_failed",
            "failed", ProductionState::Faulted, 0, 0, 0, 1);
        return ProductionInputAttemptResult::Faulted;
    };
    const char* reservedGate = ProductionReservedInputTransactionGate(
        work, binding);
    if (strcmp(reservedGate, "reserved") != 0)
        return abortPostClone(reservedGate);
    if (!ProductionReadGimmickIdentity(machineStatus,
                                        &machineIdentityAgain))
        return abortPostClone("post_clone_machine_identity_unreadable");
    if (!ProductionReadGimmickIdentity(inputChestStatus,
                                        &inputIdentityAgain))
        return abortPostClone("post_clone_input_identity_unreadable");
    if (!ProductionIdEqual(machineIdentityAgain, binding.machine))
        return abortPostClone("post_clone_machine_identity_changed");
    if (!ProductionIdEqual(inputIdentityAgain, binding.input))
        return abortPostClone("post_clone_input_identity_changed");
    const size_t preparedSlot = plan.machineSlot;
    if (!ProductionPrepareIdleMachineForInput(
            machineStatus, binding, &plan, &backingAgain,
            &machineDataAgain, gameSecond, trace))
        return abortPostClone("post_clone_machine_prepare_rejected");
    if (plan.machineSlot != preparedSlot)
        return abortPostClone("post_clone_machine_slot_changed");
    if (backingAgain != backing || machineDataAgain != machineData)
        return abortPostClone("post_clone_machine_storage_changed");
    if (!ProductionInputSourcesCurrent(inputChestStatus, plan))
        return abortPostClone("post_clone_source_current_failed");
    trace->transactionGate = "post_clone_validated";

    const u64 start = static_cast<u64>(gameSecond);
    const u64 duration = plan.recipe->duration * 60ULL;
    char idKey[8] = {};
    char countKey[9] = {};
    _snprintf_s(idKey, sizeof(idKey), _TRUNCATE, "itemID%zu",
                plan.machineSlot);
    _snprintf_s(countKey, sizeof(countKey), _TRUNCATE, "itemVal%zu",
                plan.machineSlot);
    volatile LONG64* startField = reinterpret_cast<volatile LONG64*>(
        reinterpret_cast<unsigned char*>(machineStatus) +
        PRODUCTION_TIME_VALUE_OFFSET + plan.machineSlot * sizeof(u64));
    volatile LONG64* durationField = reinterpret_cast<volatile LONG64*>(
        reinterpret_cast<unsigned char*>(machineStatus) +
        PRODUCTION_CREATE_TIME_OFFSET + plan.machineSlot * sizeof(u64));
    bool durationWritten = false;
    bool startWritten = false;
    bool itemIdTouched = false;
    bool itemCountTouched = false;
    bool inProcTouched = false;
    size_t publishedCount = 0;
    bool sourcesTouched = false;
    const auto setStatusInt = [&](const char* key, int value) {
        return plan.silentIntWrites
            ? ProductionSetStatusIntSilent(machineStatus, key, value)
            : ProductionSetStatusInt(machineStatus, key, value);
    };
    const auto setStatusU64 = [&](const char* key, u64 value) {
        return plan.silentU64Writes
            ? ProductionSetStatusU64Silent(machineStatus, key, value)
            : ProductionSetStatusU64(machineStatus, key, value);
    };

    const auto rollback = [&]() {
        bool rollbackOk = true;
        bool machineStorageCurrent =
            ProductionInputMachineStorageCurrent(
                machineStatus, binding, plan, backing, machineData);
        if (inProcTouched) {
            if (!machineStorageCurrent) {
                rollbackOk = false;
            } else {
                const bool inProcRestored = setStatusInt(
                    "inProc", static_cast<int>(plan.inProcBefore));
                rollbackOk = inProcRestored && rollbackOk;
                machineStorageCurrent = inProcRestored &&
                    ProductionInputMachineStorageCurrent(
                        machineStatus, binding, plan, backing,
                        machineData);
                if (!machineStorageCurrent) rollbackOk = false;
            }
        }
        const size_t first = plan.machineSlot *
            PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT;
        if (!machineStorageCurrent && publishedCount != 0) {
            // The status observer replaced +0x2B8.  The captured array may
            // already be freed, so never dereference it during rollback.
            rollbackOk = false;
        } else if (machineStorageCurrent) {
            for (size_t reverse = publishedCount; reverse > 0; --reverse) {
                ProductionInputIngredient& ingredient =
                    plan.ingredients[reverse - 1];
                if (!ingredient.slotPublished) continue;
                machineStorageCurrent =
                    ProductionInputMachineStorageCurrent(
                        machineStatus, binding, plan, backing, machineData);
                if (!machineStorageCurrent) {
                    rollbackOk = false;
                    break;
                }
                void** slot = &backing[first + reverse - 1];
                if (!NearbyIsReadable(slot, sizeof(void*))) {
                    rollbackOk = false;
                    continue;
                }
                void* removed = InterlockedCompareExchangePointer(
                    reinterpret_cast<void* volatile*>(slot), nullptr,
                    ingredient.clone);
                if (removed == ingredient.clone) {
                    NearbyReleaseOwnedReference(ingredient.clone);
                    ingredient.slotPublished = false;
                    machineStorageCurrent =
                        ProductionInputMachineStorageCurrent(
                            machineStatus, binding, plan, backing,
                            machineData);
                    if (!machineStorageCurrent) {
                        rollbackOk = false;
                        break;
                    }
                } else {
                    rollbackOk = false;
                }
            }
        }
        if (sourcesTouched || plan.sourceCount != 0) {
            ProductionStableId rollbackInputIdentity = {};
            const bool inputIdentityCurrent =
                ProductionReadGimmickIdentity(
                    inputChestStatus, &rollbackInputIdentity) &&
                ProductionIdEqual(rollbackInputIdentity, binding.input);
            rollbackOk = inputIdentityCurrent &&
                ProductionRollbackInputSources(inputChestStatus, &plan) &&
                rollbackOk;
        }
        if (itemCountTouched) {
            machineStorageCurrent =
                ProductionInputMachineStorageCurrent(
                    machineStatus, binding, plan, backing, machineData);
            rollbackOk = machineStorageCurrent &&
                setStatusInt(countKey, 0) &&
                rollbackOk;
        }
        if (itemIdTouched) {
            machineStorageCurrent =
                ProductionInputMachineStorageCurrent(
                    machineStatus, binding, plan, backing, machineData);
            rollbackOk = machineStorageCurrent &&
                setStatusU64(idKey, 0) &&
                rollbackOk;
        }
        if (startWritten) {
            machineStorageCurrent =
                ProductionInputMachineStorageCurrent(
                    machineStatus, binding, plan, backing, machineData);
            if (!machineStorageCurrent) {
                rollbackOk = false;
            } else {
                const LONG64 observed = InterlockedCompareExchange64(
                    startField, 0, static_cast<LONG64>(start));
                if (observed != static_cast<LONG64>(start) && observed != 0)
                    rollbackOk = false;
            }
        }
        if (durationWritten) {
            machineStorageCurrent =
                ProductionInputMachineStorageCurrent(
                    machineStatus, binding, plan, backing, machineData);
            if (!machineStorageCurrent) {
                rollbackOk = false;
            } else {
                const LONG64 observed = InterlockedCompareExchange64(
                    durationField, 0, static_cast<LONG64>(duration));
                if (observed != static_cast<LONG64>(duration) && observed != 0)
                    rollbackOk = false;
            }
        }
        const bool localsReleased = ProductionReleaseInputIngredientLocals(
            &plan, true);
        rollbackOk = localsReleased && rollbackOk;
        if (!rollbackOk) {
            ProductionReleaseInputSourceGuards(&plan, true);
            for (size_t index = 0; index < plan.ingredientCount; ++index) {
                if (plan.ingredients[index].localOwned)
                    ProductionRetainInputQuarantine(
                        plan.ingredients[index].clone);
            }
        }
        return rollbackOk;
    };

    g_productionPreUpdateStage.store(9, std::memory_order_release);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=transfer_begin "
        "result=INCOMPLETE reason=none phase=input_transaction "
        "device_map_id=%llu device_id=%llu input_map_id=%llu input_id=%llu "
        "recipe=%llu slot=%zu planned=%d ingredients=%zu "
        "status_notify=%s raw_pointer_cache=0 writes=0\n",
        static_cast<unsigned long long>(binding.machine.mapId),
        static_cast<unsigned long long>(binding.machine.uniqueId),
        static_cast<unsigned long long>(binding.input.mapId),
        static_cast<unsigned long long>(binding.input.uniqueId),
        static_cast<unsigned long long>(plan.recipe->recipeId),
        plan.machineSlot, plan.totalAmount, plan.ingredientCount,
        plan.silentIntWrites || plan.silentU64Writes
            ? "silent_no_notify" : "notify");

    const auto failTransaction = [&](const char* reason) {
        int removedBeforeRollback = 0;
        for (size_t index = 0; index < plan.sourceCount; ++index) {
            if (plan.sources[index].applied)
                removedBeforeRollback += plan.sources[index].amount;
        }
        int writtenBeforeRollback = 0;
        for (size_t index = 0; index < publishedCount; ++index)
            writtenBeforeRollback += plan.ingredients[index].amount;
        const bool rollbackOk = rollback();
        const u64 txn = g_productionTxnSequence.fetch_add(
            1, std::memory_order_relaxed) + 1;
        ProductionFaultTransfer(reason,
                                g_productionTransferQuarantine.load());
        trace->transactionGate = reason;
        trace->nextRetryGameSecond = INT64_MAX;
        ProductionLogInputTransfer(
            txn, binding, *trace, "FAIL", reason,
            rollbackOk ? "verified" : "failed", ProductionState::Faulted,
            removedBeforeRollback, writtenBeforeRollback, 0, 1);
        return ProductionInputAttemptResult::Faulted;
    };

    // Match native PushItem publication order.  inProc remains zero until
    // every timer/map/source/backing postcondition is complete.
    if (InterlockedCompareExchange64(
            durationField, static_cast<LONG64>(duration), 0) != 0)
        return failTransaction("input_duration_precommit_changed");
    durationWritten = true;
    if (InterlockedCompareExchange64(
            startField, static_cast<LONG64>(start), 0) != 0)
        return failTransaction("input_start_precommit_changed");
    startWritten = true;
    itemIdTouched = true;
    if (!setStatusU64(idKey, plan.recipe->outputItemId))
        return failTransaction("input_item_id_publish_failed");
    reservedGate = ProductionReservedInputTransactionGate(work, binding);
    if (strcmp(reservedGate, "reserved") != 0)
        return failTransaction(reservedGate);
    if (!ProductionInputMachineStorageCurrent(
            machineStatus, binding, plan, backing, machineData))
        return failTransaction("input_item_id_notify_storage_changed");
    itemCountTouched = true;
    if (!setStatusInt(countKey,
                      static_cast<int>(plan.recipe->outputCount)))
        return failTransaction("input_item_count_publish_failed");
    reservedGate = ProductionReservedInputTransactionGate(work, binding);
    if (strcmp(reservedGate, "reserved") != 0)
        return failTransaction(reservedGate);

    g_productionPreUpdateStage.store(10, std::memory_order_release);
    // Both map publications synchronously notify native observers.  Those
    // callbacks are allowed to rebuild the machine's storage.  Never carry
    // the pre-notify +0x2B8 pointer across that boundary without resolving
    // and proving the exact transitional state again.
    if (!ProductionInputMachinePreparedForBacking(
            machineStatus, binding, plan, backing, machineData,
            start, duration, gameSecond) ||
        !ProductionInputSourcesCurrent(inputChestStatus, plan))
        return failTransaction("input_post_notify_storage_changed");
    sourcesTouched = true;
    if (!ProductionApplyInputSources(
            work, binding, machineStatus, inputChestStatus, &plan, backing,
            machineData, start, duration, trace))
        return failTransaction(
            trace->transactionGate &&
                    strcmp(trace->transactionGate, "post_clone_validated") != 0
                ? trace->transactionGate
                : "input_chest_source_commit_failed");
    reservedGate = ProductionReservedInputTransactionGate(work, binding);
    if (strcmp(reservedGate, "reserved") != 0)
        return failTransaction(reservedGate);
    if (!ProductionInputMachinePreparedForBacking(
            machineStatus, binding, plan, backing, machineData,
            start, duration, gameSecond))
        return failTransaction("input_post_source_storage_changed");

    const size_t first = plan.machineSlot *
        PRODUCTION_MACHINE_INGREDIENTS_PER_SLOT;
    for (size_t index = 0; index < plan.ingredientCount; ++index) {
        ProductionInputIngredient& ingredient = plan.ingredients[index];
        void** slot = &backing[first + index];
        if (!NearbyIsReadable(slot, sizeof(void*)) || *slot != nullptr)
            return failTransaction("input_machine_backing_changed");
        NearbyAddReference(ingredient.clone); // machine-slot ownership
        void* observed = InterlockedCompareExchangePointer(
            reinterpret_cast<void* volatile*>(slot), ingredient.clone,
            nullptr);
        if (observed != nullptr) {
            NearbyReleaseOwnedReference(ingredient.clone);
            return failTransaction("input_machine_slot_publish_failed");
        }
        ingredient.slotPublished = true;
        ++publishedCount;
        if (*slot != ingredient.clone)
            return failTransaction("input_machine_slot_readback_failed");
    }

    g_productionPreUpdateStage.store(11, std::memory_order_release);
    if (!ProductionInputMachineStorageCurrent(
            machineStatus, binding, plan, backing, machineData))
        return failTransaction("input_pre_inproc_storage_changed");
    reservedGate = ProductionReservedInputTransactionGate(work, binding);
    if (strcmp(reservedGate, "reserved") != 0)
        return failTransaction(reservedGate);
    inProcTouched = true;
    if (!setStatusInt("inProc", 1))
        return failTransaction("input_inproc_publish_failed");
    reservedGate = ProductionReservedInputTransactionGate(work, binding);
    if (strcmp(reservedGate, "reserved") != 0)
        return failTransaction(reservedGate);
    if (!ProductionVerifyInputCommit(
            machineStatus, inputChestStatus, binding, plan, backing,
            machineData, start, duration))
        return failTransaction("input_postcommit_revalidation_failed");

    // Machine backing and chest slots now own their final references.  Drop
    // only the transaction-local guards after all readbacks have passed.
    ProductionReleaseInputSourceGuards(&plan, false);
    if (!ProductionReleaseInputIngredientLocals(&plan, false))
        return failTransaction("input_local_reference_release_failed");
    if (started) *started = true;
    g_productionPreUpdateStage.store(12, std::memory_order_release);
    std::int64_t exactFinish = INT64_MAX;
    if (duration <= static_cast<u64>(INT64_MAX) &&
        gameSecond <= INT64_MAX - static_cast<std::int64_t>(duration))
        exactFinish = gameSecond + static_cast<std::int64_t>(duration);
    trace->transactionGate = "committed";
    trace->nextRetryGameSecond = exactFinish;
    const u64 txn = g_productionTxnSequence.fetch_add(
        1, std::memory_order_relaxed) + 1;
    ProductionLogInputTransfer(
        txn, binding, *trace, "PASS", "none", "not_needed",
        ProductionState::Running, plan.totalAmount, plan.totalAmount, 1, 1);
    if (expectedFinishGameSecond && exactFinish != INT64_MAX)
        *expectedFinishGameSecond = exactFinish;
    return ProductionInputAttemptResult::Started;
}

struct ProductionDueResolution {
    ProductionStatusHit hits[3];
    ProductionObjectSnapshot objects[3];
    size_t hitCount;
    size_t objectCount;
    size_t registryNodes;
    bool ambiguous;
};

static std::int64_t ProductionAddGameTicks(std::int64_t now,
                                           std::int64_t delay) {
    if (now < 0 || delay < 0 || now > INT64_MAX - delay) return INT64_MAX;
    return now + delay;
}

static u32 ProductionNextRetryGameTicks(
        const ProductionBinding& previous, ProductionDueKind kind,
        std::int64_t baseTicks) {
    u64 delay = static_cast<u64>(baseTicks);
    if (previous.dueKind == kind && previous.retryGameTicks >= delay) {
        delay = static_cast<u64>(previous.retryGameTicks) * 2ULL;
    }
    if (delay > static_cast<u64>(PRODUCTION_MAX_RETRY_GAME_TICKS))
        delay = static_cast<u64>(PRODUCTION_MAX_RETRY_GAME_TICKS);
    return static_cast<u32>(delay);
}

static void ProductionInvalidateScheduleClock() {
    // Only the scalar rollback detector is cached.  The producer's atomic
    // world/load tuple makes an older clock sample ineligible automatically.
    g_productionScheduleGameSecondValid = false;
}

// Atomics-only lifecycle gate.  It runs before the published clock snapshot
// and therefore before any root, save, registry, or object read.
static bool ProductionReadSettledScheduleEpoch(u64* worldEpochOut,
                                               u64* loadGenerationOut) {
    if (!worldEpochOut || !loadGenerationOut ||
        !g_productionWorldContextActive.load(std::memory_order_acquire)) {
        ProductionInvalidateScheduleClock();
        return false;
    }
    const ULONGLONG nowTick = GetTickCount64();
    const ULONGLONG activeSince = g_productionWorldActiveSince;
    if (!activeSince || nowTick < activeSince) {
        ProductionInvalidateScheduleClock();
        return false;
    }
    const ULONGLONG worldAge = nowTick - activeSince;
    const u64 inFlight = g_autoPetLoadInFlightGeneration.load(
        std::memory_order_acquire);
    const u64 completion = g_autoPetLoadCompletionGeneration.load(
        std::memory_order_acquire);
    const u64 ready = g_autoPetLoadReadyGeneration.load(
        std::memory_order_acquire);
    const u64 handled = g_autoPetHandledLoadGeneration.load(
        std::memory_order_acquire);
    const bool loadCompleted = completion != 0 && ready == completion &&
                               handled == completion;
    const bool noLoadObserved = inFlight == 0 && completion == 0 &&
                                ready == 0 && handled == 0;
    const bool naturalMorningSettleReady =
        g_productionNaturalMorningSettleReady &&
        g_productionNaturalMorningGenerationObserved != 0;
    if (inFlight != 0 ||
        (!naturalMorningSettleReady &&
         worldAge < PRODUCTION_TRANSFER_SETTLE_MS) ||
        (!loadCompleted &&
         !naturalMorningSettleReady &&
         !(noLoadObserved &&
           worldAge >= PRODUCTION_NEW_GAME_SETTLE_MS))) {
        ProductionInvalidateScheduleClock();
        return false;
    }
    *worldEpochOut = g_productionWorldScheduleEpoch.load(
        std::memory_order_acquire);
    *loadGenerationOut = completion;
    return true;
}

static bool ProductionReadScheduleClock(u64 worldEpoch, u64 loadGeneration,
                                        std::int64_t* gameSecondOut) {
    if (!gameSecondOut ||
        !ProductionReadPublishedMainWorldGameClock(
            worldEpoch, loadGeneration, gameSecondOut)) return false;
    u64 currentWorldEpoch = 0;
    u64 currentLoadGeneration = 0;
    return ProductionReadSettledScheduleEpoch(
               &currentWorldEpoch, &currentLoadGeneration) &&
        currentWorldEpoch == worldEpoch &&
        currentLoadGeneration == loadGeneration;
}

// Called only after one due value has been selected and its live transaction
// gate is open.  Root/save pointers are callback-local and never stored in
// scheduler state; the surrounding readable-region cache is likewise fresh.
static bool ProductionReadCallbackSave(u64 worldEpoch, u64 loadGeneration,
                                       void** saveOut) {
    if (!saveOut) return false;
    *saveOut = nullptr;
    u64 currentWorldEpoch = 0;
    u64 currentLoadGeneration = 0;
    if (!ProductionReadSettledScheduleEpoch(
            &currentWorldEpoch, &currentLoadGeneration) ||
        currentWorldEpoch != worldEpoch ||
        currentLoadGeneration != loadGeneration) return false;
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    void* root = nullptr;
    void* save = nullptr;
    if (!base ||
        !ProductionReadPointer(reinterpret_cast<void*>(base + RVA_GAME_ROOT),
                               0, &root) ||
        !ProductionReadPointer(root, PRODUCTION_SAVE_OFFSET, &save) ||
        !NearbyIsReadable(save, sizeof(void*)) ||
        *reinterpret_cast<const uintptr_t*>(save) !=
            base + RVA_PRODUCTION_SAVE_DATA_VTABLE) return false;
    if (!ProductionReadSettledScheduleEpoch(&currentWorldEpoch,
                                             &currentLoadGeneration) ||
        currentWorldEpoch != worldEpoch ||
        currentLoadGeneration != loadGeneration) return false;
    *saveOut = save;
    return true;
}

static bool ProductionBindingConfigurationEqual(
        const ProductionBinding& left, const ProductionBinding& right) {
    return ProductionIdEqual(left.machine, right.machine) &&
        ProductionIdEqual(left.input, right.input) &&
        ProductionIdEqual(left.output, right.output) &&
        left.hasInput == right.hasInput &&
        left.hasOutput == right.hasOutput &&
        left.enabled == right.enabled &&
        left.committed == right.committed &&
        left.groupIndex == right.groupIndex &&
        left.nearestChestCount == right.nearestChestCount;
}

static void ProductionParkBinding(ProductionBinding* binding) {
    if (!binding) return;
    binding->dueKind = ProductionDueKind::Parked;
    binding->nextDueGameSecond = INT64_MAX;
    binding->expectedFinishGameSecond = 0;
    binding->retryGameTicks = 0;
}

static void ProductionDirtyBinding(ProductionBinding* binding) {
    if (!binding) return;
    binding->dueKind = ProductionDueKind::Dirty;
    binding->nextDueGameSecond = 0;
    binding->expectedFinishGameSecond = 0;
    binding->retryGameTicks = 0;
}

// ---- Chest rotation for nearest-chest bindings ------------------------------
// When the current input chest has no satisfiable recipe, rotate to the next
// nearest chest stored in binding->nearestChestIds[].  Only the 4 closest
// chests are eligible, preventing a machine from pulling from chests that are
// far away in a large flood-fill group.  Returns true if rotation happened.
// Returns false if all nearest chests have been tried and normal backoff
// should apply.
static bool ProductionRotateInputChest(ProductionBinding* binding,
        std::int64_t gameSecond) {
    if (!binding || binding->nearestChestCount <= 1)
        return false;
    // Stop after one full sweep of the nearest chests so we fall back
    // to the normal 30-minute backoff instead of spinning.
    if (binding->inputTriedCount >= binding->nearestChestCount) {
        binding->inputTriedCount = 0;
        return false;
    }
    const uint8_t next = (binding->inputRotation + 1) %
        binding->nearestChestCount;
    binding->inputRotation = next;
    binding->input = binding->nearestChestIds[next];
    ++binding->inputTriedCount;
    // If the new input chest is the same as the current output chest and
    // there are >= 3 nearest chests, advance one more step to avoid
    // input == output.
    if (binding->nearestChestCount >= 3 &&
        ProductionIdEqual(binding->input, binding->output)) {
        const uint8_t skip = (next + 1) %
            binding->nearestChestCount;
        binding->inputRotation = skip;
        binding->input = binding->nearestChestIds[skip];
        ++binding->inputTriedCount;
    }
    // Short retry (2 game-minutes) instead of Dirty so the binding does
    // not become immediately due on the next callback, which would cause
    // a tight rotate-every-frame loop and stutter.
    binding->dueKind = ProductionDueKind::InputRetry;
    binding->expectedFinishGameSecond = 0;
    binding->retryGameTicks =
        static_cast<u32>(PRODUCTION_TRANSIENT_INPUT_RETRY_GAME_TICKS);
    binding->nextDueGameSecond = ProductionAddGameTicks(
        gameSecond, PRODUCTION_TRANSIENT_INPUT_RETRY_GAME_TICKS);
    // Do NOT bump g_productionBindingRevision here.  Rotation only changes
    // the input chest pointer inside an existing binding; it does not add,
    // remove, enable, or disable a binding.
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=input_rotate result=PASS "
        "reason=chest_rotated rotation=%u/%u "
        "device_map_id=%llu device_id=%llu writes=0\n",
        next,
        static_cast<unsigned>(binding->nearestChestCount),
        static_cast<unsigned long long>(binding->machine.mapId),
        static_cast<unsigned long long>(binding->machine.uniqueId));
    return true;
}

// When the current output chest is full, rotate to the next nearest chest.
// Same semantics as ProductionRotateInputChest but for output.
static bool ProductionRotateOutputChest(ProductionBinding* binding,
        std::int64_t gameSecond) {
    if (!binding || binding->nearestChestCount <= 1)
        return false;
    // Stop after one full sweep so a permanently-full output group backs off.
    if (binding->outputTriedCount >= binding->nearestChestCount) {
        binding->outputTriedCount = 0;
        return false;
    }
    const uint8_t next = (binding->outputRotation + 1) %
        binding->nearestChestCount;
    binding->outputRotation = next;
    binding->output = binding->nearestChestIds[next];
    ++binding->outputTriedCount;
    // If the new output chest is the same as the current input chest and
    // there are >= 3 nearest chests, advance one more step.
    if (binding->nearestChestCount >= 3 &&
        ProductionIdEqual(binding->output, binding->input)) {
        const uint8_t skip = (next + 1) %
            binding->nearestChestCount;
        binding->outputRotation = skip;
        binding->output = binding->nearestChestIds[skip];
        ++binding->outputTriedCount;
    }
    // Short retry (2 game-minutes) instead of Dirty to avoid tight
    // rotate-every-frame loops that stutter the game.
    binding->dueKind = ProductionDueKind::InputRetry;
    binding->expectedFinishGameSecond = 0;
    binding->retryGameTicks =
        static_cast<u32>(PRODUCTION_TRANSIENT_INPUT_RETRY_GAME_TICKS);
    binding->nextDueGameSecond = ProductionAddGameTicks(
        gameSecond, PRODUCTION_TRANSIENT_INPUT_RETRY_GAME_TICKS);
    // Do NOT bump g_productionBindingRevision here (see comment in
    // ProductionRotateInputChest above for rationale).
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=output_rotate result=PASS "
        "reason=chest_rotated rotation=%u/%u "
        "device_map_id=%llu device_id=%llu writes=0\n",
        next,
        static_cast<unsigned>(binding->nearestChestCount),
        static_cast<unsigned long long>(binding->machine.mapId),
        static_cast<unsigned long long>(binding->machine.uniqueId));
    return true;
}

// A slot is an input candidate only when every one of its own timer, output
// map and eight-item backing cells is empty.  Dormant slots beyond slotLimit
// and running/finished siblings are deliberately ignored by this predicate.
static bool ProductionMachineHasIdleInputSlot(
        const ProductionObjectSnapshot* machine) {
    if (!machine || !machine->machine || !machine->processValid ||
        machine->slotLimit == 0 ||
        machine->slotLimit > PRODUCTION_MACHINE_SLOTS) return false;
    for (size_t slot = 0; slot < machine->slotLimit; ++slot) {
        if (machine->start[slot] || machine->duration[slot] ||
            machine->outputItemId[slot] || machine->outputCount[slot] != 0)
            continue;
        return true;
    }
    return false;
}

static void ProductionPrimeAllEnabledBindingsDirty(const char* reason) {
    size_t enabled = 0;
    AcquireSRWLockExclusive(&g_productionBindingLock);
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        ProductionBinding& binding = g_productionBindings[index];
        if (binding.committed && binding.enabled) {
            ProductionDirtyBinding(&binding);
            ++enabled;
        } else if (binding.committed && !binding.enabled &&
                   binding.hasOutput) {
            // Paused bindings with output: prime them too so the
            // drain-on-pause path can transfer finished products.
            ProductionDirtyBinding(&binding);
            ++enabled;
        } else {
            ProductionParkBinding(&binding);
        }
    }
    // A new lifecycle/config epoch must reconsider the first or only binding;
    // a cursor inherited from the previous epoch is not authoritative.
    g_productionOutputDueCursor = 0;
    g_productionInputDueCursor = 0;
    ReleaseSRWLockExclusive(&g_productionBindingLock);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=schedule_prime result=PASS "
        "reason=%s enabled_bindings=%zu due_kind=Dirty "
        "registry_reads=0 virtual_query=0 writes=0\n",
        reason ? reason : "lifecycle_edge", enabled);
}

// The main-world CGameTime detour publishes this edge only for a native day
// crossing, backward correction, or >= one-hour forward jump.  Consuming it
// touches Mod-owned binding values only.  Registry/save resolution remains in
// ProductionRunDueStateMachine after settle/load/callback gates are open, so a
// sleep wake cannot execute a transaction in the post-native callback which
// observed the discontinuity.
static void ProductionRefreshSchedulerNativeClockWake() {
    const u64 published =
        g_productionMainWorldGameClockWakeSequence.load(
            std::memory_order_acquire);
    if (published == g_productionScheduleClockWakeObserved) return;
    g_productionScheduleClockWakeObserved = published;
    if (published == 0) return;

    AcquireSRWLockExclusive(&g_productionBindingLock);
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        ProductionBinding& binding = g_productionBindings[index];
        if (binding.committed && binding.enabled)
            ProductionDirtyBinding(&binding);
        else if (binding.committed && !binding.enabled &&
                 binding.hasOutput)
            ProductionDirtyBinding(&binding);
        else
            ProductionParkBinding(&binding);
    }
    ReleaseSRWLockExclusive(&g_productionBindingLock);
}

static void ProductionScheduleBindingFromSnapshot(
        ProductionBinding* binding, const ProductionObjectSnapshot* machine,
        std::int64_t gameSecond) {
    if (!binding) return;
    // Allow OutputReady to proceed even when disabled (drain-on-pause).
    if (!binding->committed ||
        (!binding->enabled &&
         binding->state != ProductionState::OutputReady) ||
        binding->state == ProductionState::Idle ||
        binding->state == ProductionState::Unbound ||
        binding->state == ProductionState::Faulted) {
        ProductionParkBinding(binding);
        return;
    }
    if (binding->state == ProductionState::Running && machine) {
        std::int64_t earliest = INT64_MAX;
        const size_t slotLimit = machine->slotLimit > 0 &&
                                         machine->slotLimit <=
                                             PRODUCTION_MACHINE_SLOTS
            ? machine->slotLimit : PRODUCTION_MACHINE_SLOTS;
        for (size_t slot = 0; slot < slotLimit; ++slot) {
            const u64 start = machine->start[slot];
            const u64 duration = machine->duration[slot];
            if (!start || !duration || start > static_cast<u64>(INT64_MAX) ||
                duration > static_cast<u64>(INT64_MAX) ||
                start > static_cast<u64>(INT64_MAX) - duration) continue;
            const std::int64_t finish = static_cast<std::int64_t>(
                start + duration);
            if (finish > gameSecond && finish < earliest) earliest = finish;
        }
        // A high-level machine keeps its own per-slot timers.  While one slot
        // is still running, every lower idle slot is a separate input due that
        // the next safe callback fills one at a time; maturity of any slot
        // still wins on the following evaluation because Dirty is selected in
        // the output-priority pass first.
        if (binding->hasInput && ProductionMachineHasIdleInputSlot(machine)) {
            binding->dueKind = ProductionDueKind::InputRetry;
            binding->nextDueGameSecond = gameSecond;
            binding->expectedFinishGameSecond =
                earliest == INT64_MAX ? 0 : earliest;
            binding->retryGameTicks = 0;
            return;
        }
        if (earliest == INT64_MAX) {
            const u32 retry = ProductionNextRetryGameTicks(
                *binding, ProductionDueKind::Dirty,
                PRODUCTION_RESOLVE_RETRY_GAME_TICKS);
            binding->dueKind = ProductionDueKind::Dirty;
            binding->nextDueGameSecond = ProductionAddGameTicks(
                gameSecond, retry);
            binding->expectedFinishGameSecond = 0;
            binding->retryGameTicks = retry;
        } else {
            binding->dueKind = ProductionDueKind::Finish;
            binding->nextDueGameSecond = earliest;
            binding->expectedFinishGameSecond = earliest;
            binding->retryGameTicks = 0;
        }
        return;
    }
    if (binding->state == ProductionState::OutputReady) {
        binding->dueKind = ProductionDueKind::OutputRetry;
        binding->nextDueGameSecond = gameSecond;
        binding->expectedFinishGameSecond = 0;
        binding->retryGameTicks = 0;
        return;
    }
    if (binding->state == ProductionState::WaitingOutputSpace) {
        binding->dueKind = ProductionDueKind::OutputRetry;
        binding->retryGameTicks = static_cast<u32>(
            PRODUCTION_OUTPUT_RETRY_GAME_TICKS);
        binding->nextDueGameSecond = ProductionAddGameTicks(
            gameSecond, PRODUCTION_OUTPUT_RETRY_GAME_TICKS);
        binding->expectedFinishGameSecond = 0;
        return;
    }
    if (binding->state == ProductionState::WaitingInput) {
        binding->dueKind = ProductionDueKind::InputRetry;
        binding->nextDueGameSecond = gameSecond;
        binding->expectedFinishGameSecond = 0;
        binding->retryGameTicks = 0;
        return;
    }
    ProductionDirtyBinding(binding);
}

static void ProductionRefreshSchedulerEpochs() {
    const u64 revision = g_productionBindingRevision.load(
        std::memory_order_acquire);
    const u64 loadInFlight = g_autoPetLoadInFlightGeneration.load(
        std::memory_order_acquire);
    const u64 loadCompletion = g_autoPetLoadCompletionGeneration.load(
        std::memory_order_acquire);
    const u64 loadReady = g_autoPetLoadReadyGeneration.load(
        std::memory_order_acquire);
    const u64 loadHandled = g_autoPetHandledLoadGeneration.load(
        std::memory_order_acquire);
    const u64 worldEpoch = g_productionWorldScheduleEpoch.load(
        std::memory_order_acquire);
    if (revision == g_productionScheduleRevisionObserved &&
        loadInFlight == g_productionScheduleLoadInFlightObserved &&
        loadCompletion == g_productionScheduleLoadGenerationObserved &&
        loadReady == g_productionScheduleLoadReadyObserved &&
        loadHandled == g_productionScheduleLoadHandledObserved &&
        worldEpoch == g_productionScheduleWorldEpochObserved) return;

    const bool loadEdge =
        loadInFlight != g_productionScheduleLoadInFlightObserved ||
        loadCompletion != g_productionScheduleLoadGenerationObserved ||
        loadReady != g_productionScheduleLoadReadyObserved ||
        loadHandled != g_productionScheduleLoadHandledObserved;
    const bool worldEdge =
        worldEpoch != g_productionScheduleWorldEpochObserved;
    if (loadEdge) {
        // Each real load lifecycle edge starts a fresh quiet window.  Dirty is
        // published now, while registry resolution remains pending until the
        // final ready/handled generation has stayed settled for five seconds.
        g_productionWorldActiveSince = GetTickCount64();
        g_productionNaturalMorningSettleReady = false;
        g_productionNaturalMorningEdgePending = 0;
        ProductionArmTransferCooldown(PRODUCTION_TRANSFER_SETTLE_MS);
        ProductionInvalidateScheduleClock();
        g_productionScheduleGameSecondValid = false;
    }
    if (loadEdge || worldEdge)
        ProductionInvalidatePersistentReadableCache();
    ProductionPrimeAllEnabledBindingsDirty(
        loadEdge ? "load_generation_edge" :
        (worldEdge ? "world_context_edge" : "binding_revision_edge"));
    g_productionScheduleRevisionObserved =
        g_productionBindingRevision.load(std::memory_order_acquire);
    g_productionScheduleLoadInFlightObserved =
        g_autoPetLoadInFlightGeneration.load(std::memory_order_acquire);
    g_productionScheduleLoadGenerationObserved =
        g_autoPetLoadCompletionGeneration.load(std::memory_order_acquire);
    g_productionScheduleLoadReadyObserved =
        g_autoPetLoadReadyGeneration.load(std::memory_order_acquire);
    g_productionScheduleLoadHandledObserved =
        g_autoPetHandledLoadGeneration.load(std::memory_order_acquire);
    g_productionScheduleWorldEpochObserved =
        g_productionWorldScheduleEpoch.load(std::memory_order_acquire);
}

// Native time normally advances monotonically inside one settled world/load
// epoch, but debug clock changes and lifecycle gaps can still move it backward.
// A binding scheduled against the old future must not remain asleep until that
// timestamp is reached again.  This edge touches binding values only: it does
// not resolve the save registry, query page protections, or emit a poll log.
static void ProductionRefreshSchedulerGameSecond(
        u64 worldEpoch, u64 loadGeneration, std::int64_t gameSecond) {
    if (!g_productionScheduleGameSecondValid ||
        g_productionScheduleGameSecondWorldEpoch != worldEpoch ||
        g_productionScheduleGameSecondLoadGeneration != loadGeneration) {
        g_productionScheduleGameSecondObserved = gameSecond;
        g_productionScheduleGameSecondWorldEpoch = worldEpoch;
        g_productionScheduleGameSecondLoadGeneration = loadGeneration;
        g_productionScheduleGameSecondValid = true;
        return;
    }
    const bool movedBackward =
        gameSecond < g_productionScheduleGameSecondObserved;
    g_productionScheduleGameSecondObserved = gameSecond;
    if (!movedBackward) return;
    AcquireSRWLockExclusive(&g_productionBindingLock);
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        ProductionBinding& binding = g_productionBindings[index];
        if (binding.committed && binding.enabled)
            ProductionDirtyBinding(&binding);
        else if (binding.committed && !binding.enabled &&
                 binding.hasOutput)
            ProductionDirtyBinding(&binding);
        else
            ProductionParkBinding(&binding);
    }
    ReleaseSRWLockExclusive(&g_productionBindingLock);
}

static bool ProductionSelectDueBinding(std::int64_t gameSecond,
                                       u64 worldEpoch, u64 loadGeneration,
                                       ProductionDueWork* work) {
    if (!work || gameSecond < 0) return false;
    *work = {};
    AcquireSRWLockShared(&g_productionBindingLock);
    const size_t count = g_productionBindingCount;
    const auto selectPass = [&](bool outputPriority, size_t* cursor) {
        if (!count) return false;
        const size_t first = *cursor < count ? *cursor : 0;
        for (size_t offset = 0; offset < count; ++offset) {
            const size_t index = (first + offset) % count;
            const ProductionBinding& binding = g_productionBindings[index];
            if (!binding.committed ||
                (!binding.enabled &&
                 binding.state != ProductionState::OutputReady) ||
                binding.dueKind == ProductionDueKind::Parked ||
                binding.nextDueGameSecond > gameSecond) continue;
            // InputRetry is the only due kind that may ride a Running binding:
            // a high-level machine fills its lower idle slots one callback at a
            // time while its occupied slots continue running.  The output pass
            // must never swallow that kind, and Dirty re-evaluations remain
            // output-class so a newly matured slot always wins first.
            const bool inputDue =
                binding.dueKind == ProductionDueKind::InputRetry;
            const bool isInputClass =
                inputDue || binding.state == ProductionState::WaitingInput;
            const bool isOutputClass = !inputDue &&
                (binding.dueKind == ProductionDueKind::Dirty ||
                 binding.dueKind == ProductionDueKind::Finish ||
                 binding.dueKind == ProductionDueKind::OutputRetry ||
                 binding.state == ProductionState::Starting ||
                 binding.state == ProductionState::Running ||
                 binding.state == ProductionState::OutputReady ||
                 binding.state == ProductionState::WaitingOutputSpace);
            if ((outputPriority && !isOutputClass) ||
                (!outputPriority && (isOutputClass || !isInputClass)))
                continue;
            work->binding = binding;
            work->revision = g_productionBindingRevision.load(
                std::memory_order_acquire);
            work->requiredCompletedSequence =
                g_productionTransferMutationSequence.load(
                    std::memory_order_acquire);
            // Carry the exact epoch pair which validated the scalar clock
            // snapshot.  A callback-local save is resolved only after this
            // due value wins and the same pair passes the live gate again.
            work->loadGeneration = loadGeneration;
            work->worldEpoch = worldEpoch;
            work->outputPriority = outputPriority;
            *cursor = (index + 1) % count;
            return true;
        }
        return false;
    };
    bool found = selectPass(true, &g_productionOutputDueCursor);
    if (!found)
        found = selectPass(false, &g_productionInputDueCursor);
    ReleaseSRWLockShared(&g_productionBindingLock);
    return found;
}

static bool ProductionHasScheduledBinding() {
    bool scheduled = false;
    AcquireSRWLockShared(&g_productionBindingLock);
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        const ProductionBinding& binding = g_productionBindings[index];
        if (binding.committed && binding.enabled &&
            binding.dueKind != ProductionDueKind::Parked) {
            scheduled = true;
            break;
        }
    }
    ReleaseSRWLockShared(&g_productionBindingLock);
    return scheduled;
}

// Resolve at most the selected binding's three stable identities.  The
// canonical list is visited only after a due binding has been selected; all
// status pointers remain callback-local and are discarded on return.
//
// The game's gimmick registry is a linked list whose nodes live in a handful
// of committed heap regions.  A full identity walk is cheap only after the
// readable-region cache is warm, and a fresh callback-local cache is rebuilt
// on every transaction callback.  Paying one full walk per due binding turns
// the morning prime into a visible frame-drop spike, so the walk's *ordinal
// positions* are cached between callbacks (space for time).  Only integer
// indices and stable ids are cached; raw status pointers are never retained
// across callbacks and every cached index is revalidated by walking the list
// and re-reading the identity before use.
struct ProductionRegistryIndexCacheEntry {
    u64 mapId;
    u64 uniqueId;
    u32 index;
    u32 padding;
};
static constexpr size_t PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY = 4096;
static ProductionRegistryIndexCacheEntry
    g_productionRegistryIndexBuild[PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY];
static ProductionRegistryIndexCacheEntry
    g_productionRegistryIndexCache[PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY];
static size_t g_productionRegistryIndexCacheCount = 0;
static u64 g_productionRegistryIndexCacheLoadGeneration = 0;
static bool g_productionRegistryIndexCacheTruncated = false;
static SRWLOCK g_productionRegistryIndexCacheLock = SRWLOCK_INIT;

// Comparison function for sorting cache entries by (mapId, uniqueId) for
// binary search.  Entries with equal keys retain their relative order
// (stable sort via qsort is not guaranteed, but duplicate keys are handled
// by the lookup function's adjacency check).
static int ProductionRegistryIndexCacheCompare(
        const void* a, const void* b) {
    const ProductionRegistryIndexCacheEntry* ea =
        static_cast<const ProductionRegistryIndexCacheEntry*>(a);
    const ProductionRegistryIndexCacheEntry* eb =
        static_cast<const ProductionRegistryIndexCacheEntry*>(b);
    if (ea->mapId < eb->mapId) return -1;
    if (ea->mapId > eb->mapId) return 1;
    if (ea->uniqueId < eb->uniqueId) return -1;
    if (ea->uniqueId > eb->uniqueId) return 1;
    return 0;
}

static void ProductionRegistryIndexCachePublish(
        const ProductionRegistryIndexCacheEntry* entries, size_t count,
        size_t declared, u64 loadGeneration) {
    if (!entries) count = 0;
    const size_t copyCount = count < PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY
        ? count : PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY;
    // Sort into the reusable build buffer (static, not stack) by (mapId,
    // uniqueId) so the lookup can use binary search.  When the caller
    // already passes g_productionRegistryIndexBuild (the common case),
    // the copy is a no-op self-assignment.
    if (copyCount && entries != g_productionRegistryIndexBuild)
        memcpy(g_productionRegistryIndexBuild, entries,
               copyCount * sizeof(ProductionRegistryIndexCacheEntry));
    if (copyCount > 1)
        qsort(g_productionRegistryIndexBuild, copyCount,
              sizeof(ProductionRegistryIndexCacheEntry),
              ProductionRegistryIndexCacheCompare);
    AcquireSRWLockExclusive(&g_productionRegistryIndexCacheLock);
    if (g_productionRegistryIndexCacheLoadGeneration != loadGeneration) {
        g_productionRegistryIndexCacheCount = 0;
    }
    if (copyCount)
        memcpy(g_productionRegistryIndexCache,
               g_productionRegistryIndexBuild,
               copyCount * sizeof(ProductionRegistryIndexCacheEntry));
    g_productionRegistryIndexCacheCount = copyCount;
    g_productionRegistryIndexCacheDeclared = declared;
    g_productionRegistryIndexCacheLoadGeneration = loadGeneration;
    g_productionRegistryIndexCacheTruncated =
        count > PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY;
    ReleaseSRWLockExclusive(&g_productionRegistryIndexCacheLock);
}

// Returns one cached ordinal and whether the cached walk saw exactly one node
// with that stable id.  Duplicates force the caller onto the full walk.
static bool ProductionRegistryIndexCacheLookup(
        const ProductionStableId& id, size_t declared, u64 loadGeneration,
        size_t* indexOut, bool* uniqueOut) {
    if (!indexOut || !uniqueOut) return false;
    AcquireSRWLockShared(&g_productionRegistryIndexCacheLock);
    if (g_productionRegistryIndexCacheLoadGeneration != loadGeneration ||
        g_productionRegistryIndexCacheDeclared != declared ||
        g_productionRegistryIndexCacheTruncated) {
        ReleaseSRWLockShared(&g_productionRegistryIndexCacheLock);
        return false;
    }
    // Binary search for (mapId, uniqueId) in the sorted cache.
    size_t lo = 0;
    size_t hi = g_productionRegistryIndexCacheCount;
    bool found = false;
    u32 cachedOrdinal = 0;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        const ProductionRegistryIndexCacheEntry& entry =
            g_productionRegistryIndexCache[mid];
        if (entry.mapId < id.mapId ||
            (entry.mapId == id.mapId && entry.uniqueId < id.uniqueId)) {
            lo = mid + 1;
        } else if (entry.mapId > id.mapId ||
                   (entry.mapId == id.mapId && entry.uniqueId > id.uniqueId)) {
            hi = mid;
        } else {
            // Found a match; scan neighbors for duplicates.
            found = true;
            cachedOrdinal = entry.index;
            // Check if any adjacent entries also match (duplicate keys).
            bool hasDup = false;
            if (mid > 0) {
                const ProductionRegistryIndexCacheEntry& prev =
                    g_productionRegistryIndexCache[mid - 1];
                if (prev.mapId == id.mapId && prev.uniqueId == id.uniqueId)
                    hasDup = true;
            }
            if (!hasDup && mid + 1 < g_productionRegistryIndexCacheCount) {
                const ProductionRegistryIndexCacheEntry& next =
                    g_productionRegistryIndexCache[mid + 1];
                if (next.mapId == id.mapId && next.uniqueId == id.uniqueId)
                    hasDup = true;
            }
            *uniqueOut = !hasDup;
            break;
        }
    }
    ReleaseSRWLockShared(&g_productionRegistryIndexCacheLock);
    if (!found) return false;
    *indexOut = cachedOrdinal;
    return true;
}

// Walk the live linked list exactly once to a small set of cached ordinals and
// hand back those nodes' status pointers.  Pointer values are callback-local
// and every node is validated against its predecessor; identities are re-read
// by the caller.  One pass to the farthest ordinal replaces three separate
// head walks for the machine/input/output triple.
static bool ProductionRegistryStatusesAtIndices(
        void* sentinel, size_t declared, const size_t* indices,
        size_t indexCount, void** statusesOut) {
    if (!sentinel || !indices || !statusesOut || indexCount == 0 ||
        indexCount > 3) return false;
    size_t farthest = 0;
    for (size_t entry = 0; entry < indexCount; ++entry) {
        if (indices[entry] >= declared) return false;
        if (indices[entry] > farthest) farthest = indices[entry];
        statusesOut[entry] = nullptr;
    }
    void* node = *reinterpret_cast<void**>(sentinel);
    void* previous = sentinel;
    for (size_t index = 0; index <= farthest; ++index) {
        if (!node || node == sentinel ||
            !NearbyIsReadable(node, sizeof(void*) * 3) ||
            *reinterpret_cast<void**>(
                reinterpret_cast<unsigned char*>(node) + sizeof(void*)) !=
                previous)
            return false;
        for (size_t entry = 0; entry < indexCount; ++entry) {
            if (indices[entry] != index) continue;
            statusesOut[entry] = *reinterpret_cast<void**>(
                reinterpret_cast<unsigned char*>(node) + sizeof(void*) * 2);
        }
        previous = node;
        node = *reinterpret_cast<void**>(node);
    }
    for (size_t entry = 0; entry < indexCount; ++entry) {
        if (!statusesOut[entry]) return false;
    }
    return true;
}

static bool ProductionResolveDueBinding(
        void* save, u64 loadGeneration, const ProductionBinding& binding,
        ProductionDueResolution* resolution,
        NearbyReadableRegionCache* readableCache) {
    if (!save || !resolution) return false;
    *resolution = {};
    g_productionResolveDiag = {};
    g_productionResolveDiag.registryBegin = NearbyPerformanceCounter();
    ProductionStableId wanted[3] = {binding.machine, binding.input,
                                    binding.output};
    bool wantedValid[3] = {true, binding.hasInput, binding.hasOutput};
    size_t matches[3] = {};
    void* statuses[3] = {};
    void* sentinel = nullptr;
    size_t declared = 0;
    if (!ProductionReadRegistryHead(save, &sentinel, &declared)) return false;
    ProductionEnsurePersistentReadableCache(
        readableCache, declared, loadGeneration);

    // Fast path: reuse cached ordinal positions from a previous full walk of
    // this exact save generation.  One head-to-farthest traversal revalidates
    // all three positions; identities are re-read and no pointer leaves the
    // callback.
    bool allFound = true;
    size_t cachedIndices[3] = {SIZE_MAX, SIZE_MAX, SIZE_MAX};
    size_t cachedEntryCount = 0;
    size_t cachedSlots[3] = {};
    for (size_t want = 0; want < 3; ++want) {
        if (!wantedValid[want]) continue;
        size_t cachedIndex = SIZE_MAX;
        bool cachedUnique = false;
        if (!ProductionRegistryIndexCacheLookup(
                wanted[want], declared, loadGeneration, &cachedIndex,
                &cachedUnique) ||
            !cachedUnique) {
            allFound = false;
            continue;
        }
        cachedIndices[cachedEntryCount] = cachedIndex;
        cachedSlots[cachedEntryCount] = want;
        ++cachedEntryCount;
    }
    if (allFound) {
        size_t farthestCached = 0;
        for (size_t entry = 0; entry < cachedEntryCount; ++entry) {
            if (cachedIndices[entry] > farthestCached)
                farthestCached = cachedIndices[entry];
        }
        g_productionResolveDiag.walkedNodes = farthestCached + 1;
        g_productionResolveDiag.identityReads = cachedEntryCount;
        void* cachedStatuses[3] = {};
        if (!ProductionRegistryStatusesAtIndices(
                sentinel, declared, cachedIndices, cachedEntryCount,
                cachedStatuses)) {
            allFound = false;
        } else {
            for (size_t entry = 0; entry < cachedEntryCount; ++entry) {
                const size_t want = cachedSlots[entry];
                ProductionStableId identity = {};
                if (!ProductionReadGimmickIdentity(
                        cachedStatuses[entry], &identity) ||
                    !ProductionIdEqual(identity, wanted[want])) {
                    allFound = false;
                    break;
                }
                statuses[want] = cachedStatuses[entry];
                matches[want] = 1;
            }
        }
    }
    if (allFound) {
        resolution->registryNodes = declared;
    } else {
        // Rebuild path.  One full walk refreshes the ordinal cache for every
        // registered gimmick, so the remaining morning-prime bindings resolve
        // with three identity reads instead of a whole registry walk each.
        if (g_productionFallbackAttemptLoadGeneration != loadGeneration ||
            g_productionFallbackAttemptDeclared != declared) {
            g_productionFallbackAttemptLoadGeneration = loadGeneration;
            g_productionFallbackAttemptDeclared = declared;
            g_productionFallbackAttemptCount = 0;
        }
        ++g_productionFallbackAttemptCount;
        g_productionResolveDiag.fallbackWalk = true;
        g_productionResolveDiag.fallbackAttempt =
            g_productionFallbackAttemptCount;
        g_productionResolveDiag.fallbackFirst =
            g_productionFallbackAttemptCount == 1;
        memset(matches, 0, sizeof(matches));
        memset(statuses, 0, sizeof(statuses));
        resolution->ambiguous = false;
        size_t buildCount = 0;
        void* node = *reinterpret_cast<void**>(sentinel);
        void* previous = sentinel;
        size_t visited = 0;
        for (; visited < declared; ++visited) {
            if (!node || node == sentinel ||
                !NearbyIsReadable(node, sizeof(void*) * 3) ||
                *reinterpret_cast<void**>(
                    reinterpret_cast<unsigned char*>(node) + sizeof(void*)) !=
                    previous) return false;
            void* status = *reinterpret_cast<void**>(
                reinterpret_cast<unsigned char*>(node) + sizeof(void*) * 2);
            ProductionStableId identity = {};
            if (ProductionReadGimmickIdentity(status, &identity)) {
                ++g_productionResolveDiag.identityReads;
                if (buildCount <
                        PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY &&
                    visited <= static_cast<size_t>(UINT32_MAX)) {
                    ProductionRegistryIndexCacheEntry& entry =
                        g_productionRegistryIndexBuild[buildCount++];
                    entry.mapId = identity.mapId;
                    entry.uniqueId = identity.uniqueId;
                    entry.index = static_cast<u32>(visited);
                    entry.padding = 0;
                } else if (buildCount ==
                           PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY) {
                    ++buildCount; // remember truncation for publication
                }
                for (size_t want = 0; want < 3; ++want) {
                    if (!wantedValid[want] ||
                        !ProductionIdEqual(identity, wanted[want])) continue;
                    if (++matches[want] == 1) statuses[want] = status;
                    else resolution->ambiguous = true;
                }
            }
            previous = node;
            node = *reinterpret_cast<void**>(node);
        }
        resolution->registryNodes = visited;
        if (node != sentinel) return false;
        g_productionResolveDiag.walkedNodes = visited;
        g_productionResolveDiag.registryEnd = NearbyPerformanceCounter();
        g_productionResolveDiag.registryCachePublishEntries =
            buildCount < PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY
                ? buildCount : PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY;
        ProductionRegistryIndexCachePublish(
            g_productionRegistryIndexBuild, buildCount, declared,
            loadGeneration);
    }
    if (!g_productionResolveDiag.registryEnd)
        g_productionResolveDiag.registryEnd = NearbyPerformanceCounter();
    g_productionResolveDiag.objectBegin = NearbyPerformanceCounter();
    for (size_t want = 0; want < 3; ++want) {
        if (!wantedValid[want] || matches[want] != 1) continue;
        // When input == output (same chest for both), skip the duplicate to
        // avoid adding the same object twice.  ProductionFindObject would
        // otherwise see 2 matches and return nullptr -> AmbiguousIdentity ->
        // FAULTED.  The machine (want=0) is always added; input (want=1) and
        // output (want=2) that share the same status pointer are deduplicated.
        if (want >= 1) {
            bool dup = false;
            for (size_t prev = 0; prev < want; ++prev) {
                if (wantedValid[prev] && matches[prev] == 1 &&
                    statuses[prev] == statuses[want]) {
                    dup = true;
                    break;
                }
            }
            if (dup) continue;
        }
        ProductionObjectSnapshot object = {};
        if (!ProductionReadObject(statuses[want], &object, false, false))
            return false;
        resolution->hits[resolution->hitCount++] = {wanted[want],
                                                     statuses[want]};
        resolution->objects[resolution->objectCount++] = object;
    }
    g_productionResolveDiag.objectEnd = NearbyPerformanceCounter();
    return true;
}

static bool ProductionCommitDueEvaluation(
        const ProductionDueWork& work, const ProductionBinding& evaluated,
        const ProductionObjectSnapshot* machine, std::int64_t gameSecond,
        ProductionBinding* committed) {
    bool result = false;
    AcquireSRWLockExclusive(&g_productionBindingLock);
    if (g_productionBindingRevision.load(std::memory_order_acquire) ==
            work.revision) {
        ProductionBinding* live = ProductionFindBindingLocked(
            work.binding.machine, false);
        if (live && ProductionBindingConfigurationEqual(*live,
                                                         work.binding)) {
            live->state = evaluated.state;
            live->reason = evaluated.reason;
            ProductionScheduleBindingFromSnapshot(live, machine, gameSecond);
            if (committed) *committed = *live;
            result = true;
        }
    }
    ReleaseSRWLockExclusive(&g_productionBindingLock);
    if (work.binding.state == ProductionState::Running &&
        work.binding.dueKind == ProductionDueKind::Dirty &&
        work.binding.expectedFinishGameSecond > gameSecond) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=input_post_native "
            "result=%s reason=%s device_map_id=%llu device_id=%llu "
            "game_second=%lld expected_finish_game_second=%lld "
            "native_state=%s commit_current=%d raw_pointer_cache=0 writes=0\n",
            result && evaluated.state == ProductionState::Running
                ? "PASS" : "FAIL",
            !result ? "binding_revision_changed" :
            (evaluated.state == ProductionState::Running
                ? "native_running_accepted" : "native_running_rejected"),
            static_cast<unsigned long long>(work.binding.machine.mapId),
            static_cast<unsigned long long>(work.binding.machine.uniqueId),
            static_cast<long long>(gameSecond),
            static_cast<long long>(work.binding.expectedFinishGameSecond),
            ProductionStateName(evaluated.state), result ? 1 : 0);
    }
    return result;
}

// Cheap pre-resolve gate and final reservation gate use the same atomics.  A
// newly armed settle window must leave the due timestamp untouched; otherwise
// the first post-output input would be converted into a long resolve backoff.
static const char* ProductionDueTransactionGateReason(
        const ProductionDueWork& work) {
    const u64 completed = g_productionMainCallbackCompletedSequence.load(
        std::memory_order_acquire);
    const u64 required = g_productionTransferMutationSequence.load(
        std::memory_order_acquire);
    const u64 load = g_autoPetLoadCompletionGeneration.load(
        std::memory_order_acquire);
    if (!ProductionFeatureIsEnabled()) return "feature_disabled";
    if (!g_productionWorldContextActive.load(std::memory_order_acquire))
        return "world_inactive";
    if (g_productionRegistryScanRequested.load(std::memory_order_acquire) ||
        g_productionScanInProgress.load(std::memory_order_acquire) ||
        g_productionScanActive || g_productionScanning)
        return "scan_in_progress";
    if (g_productionWorldScheduleEpoch.load(std::memory_order_acquire) !=
            work.worldEpoch) return "world_epoch_changed";
    if (g_productionBindingMutationInProgress.load(
            std::memory_order_acquire)) return "binding_mutation_in_progress";
    if (g_productionBindingRevision.load(std::memory_order_acquire) !=
            work.revision) return "binding_revision_changed";
    if (required != work.requiredCompletedSequence)
        return "callback_requirement_changed";
    if (completed < required) return "completed_callback_pending";
    if (GetTickCount64() < g_productionTransferNotBeforeTick.load(
            std::memory_order_acquire)) return "not_before_pending";
    if (g_autoPetLoadInFlightGeneration.load(std::memory_order_acquire) != 0)
        return "load_in_flight";
    if (load != work.loadGeneration) return "load_completion_changed";
    if (g_autoPetLoadReadyGeneration.load(std::memory_order_acquire) != load)
        return "load_ready_changed";
    if (g_autoPetHandledLoadGeneration.load(std::memory_order_acquire) != load)
        return "load_handled_changed";
    if (g_productionTransferInFlight.load(std::memory_order_acquire))
        return "transfer_in_flight";
    return "open";
}

static bool ProductionDueTransactionGateOpen(
        const ProductionDueWork& work) {
    return strcmp(ProductionDueTransactionGateReason(work), "open") == 0;
}

static bool ProductionReserveDueTransaction(
        const ProductionDueWork& work, const ProductionBinding& candidate,
        bool inputAction, const char** reasonOut = nullptr) {
    bool reservation = false;
    const char* reason = ProductionDueTransactionGateReason(work);
    AcquireSRWLockShared(&g_productionBindingLock);
    if (strcmp(reason, "open") == 0) {
        for (size_t index = 0; index < g_productionBindingCount; ++index) {
            const bool matches = inputAction
                ? ProductionBindingMatchesInput(g_productionBindings[index],
                                                candidate)
                : ProductionBindingMatchesTransfer(
                      g_productionBindings[index], candidate);
            if (!matches) continue;
            g_productionTransferInFlight.store(true,
                                                std::memory_order_release);
            reservation = true;
            reason = "reserved";
            break;
        }
        if (!reservation) reason = "binding_state_changed";
    }
    ReleaseSRWLockShared(&g_productionBindingLock);
    if (reasonOut) *reasonOut = reason;
    return reservation;
}

static void ProductionPublishDueActiveState() {
    const ProductionStableId active = ProductionGetActiveMachine();
    if (!active.mapId || !active.uniqueId) return;
    AcquireSRWLockShared(&g_productionBindingLock);
    ProductionState state = ProductionState::Unbound;
    ProductionReason reason = ProductionReason::InputUnbound;
    bool found = false;
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        if (!ProductionIdEqual(g_productionBindings[index].machine, active))
            continue;
        state = g_productionBindings[index].state;
        reason = g_productionBindings[index].reason;
        found = true;
        break;
    }
    ReleaseSRWLockShared(&g_productionBindingLock);
    if (!found) return;
    AcquireSRWLockExclusive(&g_productionPublishLock);
    if (g_productionPublished.activeMachinePresent &&
        ProductionIdEqual(g_productionPublished.activeMachine, active)) {
        g_productionPublished.activeState = state;
        g_productionPublished.activeReason = reason;
    }
    ReleaseSRWLockExclusive(&g_productionPublishLock);
}

static void ProductionUpdateDueBindingAfterAttempt(
        const ProductionDueWork& work, ProductionState state,
        ProductionReason reason, const ProductionObjectSnapshot* machine,
        std::int64_t gameSecond, u64 callbackSequence,
        std::int64_t exactFinish = 0) {
    ProductionDueKind scheduledKind = ProductionDueKind::Parked;
    std::int64_t scheduledSecond = INT64_MAX;
    bool scheduled = false;
    AcquireSRWLockExclusive(&g_productionBindingLock);
    if (g_productionBindingRevision.load(std::memory_order_acquire) ==
            work.revision) {
        ProductionBinding* live = ProductionFindBindingLocked(
            work.binding.machine, false);
        if (live && ProductionBindingConfigurationEqual(*live,
                                                         work.binding)) {
            live->state = state;
            live->reason = reason;
            if (state == ProductionState::Running && exactFinish > gameSecond) {
                // The original native update runs after this Mod transaction.
                // Re-resolve this one stable binding on the next safe callback
                // to prove that native update accepted inProc/timer/map state;
                // a confirmed Running snapshot is then scheduled as Finish.
                live->dueKind = ProductionDueKind::Dirty;
                live->nextDueGameSecond = gameSecond;
                live->expectedFinishGameSecond = exactFinish;
                live->retryGameTicks = 0;
            } else {
                ProductionScheduleBindingFromSnapshot(live, machine,
                                                      gameSecond);
            }
            scheduledKind = live->dueKind;
            scheduledSecond = live->nextDueGameSecond;
            scheduled = true;
        }
    }
    ReleaseSRWLockExclusive(&g_productionBindingLock);
    if (scheduled && state == ProductionState::WaitingInput) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=post_output_schedule "
            "result=PASS reason=output_first device_map_id=%llu "
            "device_id=%llu due_kind=%s game_second=%lld "
            "next_due_game_second=%lld same_callback_input=0 "
            "callback_sequence=%llu "
            "raw_pointer_cache=0 writes=0\n",
            static_cast<unsigned long long>(work.binding.machine.mapId),
            static_cast<unsigned long long>(work.binding.machine.uniqueId),
            ProductionDueKindName(scheduledKind),
            static_cast<long long>(gameSecond),
            static_cast<long long>(scheduledSecond),
            static_cast<unsigned long long>(callbackSequence));
    }
    ProductionPublishDueActiveState();
}

static u32 ProductionBackoffDueBinding(
        const ProductionDueWork& work, ProductionState state,
        ProductionReason reason, ProductionDueKind kind,
        std::int64_t baseTicks, std::int64_t gameSecond) {
    const u32 retry = ProductionNextRetryGameTicks(
        work.binding, kind, baseTicks);
    AcquireSRWLockExclusive(&g_productionBindingLock);
    if (g_productionBindingRevision.load(std::memory_order_acquire) ==
            work.revision) {
        ProductionBinding* live = ProductionFindBindingLocked(
            work.binding.machine, false);
        if (live && ProductionBindingConfigurationEqual(*live,
                                                         work.binding)) {
            live->state = state;
            live->reason = reason;
            if (state == ProductionState::Faulted) {
                ProductionParkBinding(live);
            } else {
                live->dueKind = kind;
                live->retryGameTicks = retry;
                live->expectedFinishGameSecond = 0;
                live->nextDueGameSecond = ProductionAddGameTicks(
                    gameSecond, retry);
            }
        }
    }
    ReleaseSRWLockExclusive(&g_productionBindingLock);
    ProductionPublishDueActiveState();
    return retry;
}

static void ProductionRunDueStateMachine(u64 callbackSequence) {
    // One callback-local readable-region cache serves resolution, evaluation
    // and the single transaction.  It is seeded from the scheduler's own
    // cross-callback committed-region cache (invalidated on world/load edges
    // and registry size changes) so a steady-state due callback performs
    // zero new page-protection queries.
    NearbyReadableRegionCache dueReadableCache =
        g_productionPersistentReadableCache;
    dueReadableCache.checks = 0;
    dueReadableCache.cacheHits = 0;
    dueReadableCache.virtualQueries = 0;
    dueReadableCache.virtualQueryTicks = 0;
    dueReadableCache.lastHit = 0;
    ProductionDueTimingScope dueTiming(callbackSequence, &dueReadableCache);
    u64 worldEpoch = 0;
    u64 loadGeneration = 0;
    if (!ProductionReadSettledScheduleEpoch(&worldEpoch, &loadGeneration))
        return;
    if (!ProductionHasScheduledBinding()) return;
    std::int64_t gameSecond = 0;
    if (!ProductionReadScheduleClock(worldEpoch, loadGeneration,
                                     &gameSecond)) return;
    if (gameSecond < 0) return;
    dueTiming.gameSecond = gameSecond;
    ProductionRefreshSchedulerGameSecond(worldEpoch, loadGeneration,
                                         gameSecond);
    ProductionDueWork work = {};
    if (!ProductionSelectDueBinding(gameSecond, worldEpoch, loadGeneration,
                                    &work)) return;
    dueTiming.selected = NearbyPerformanceCounter();
    const bool selectedInputWork = !work.outputPriority;
    ProductionInputAttemptTrace inputTrace = {};
    if (selectedInputWork) {
        ProductionInitializeInputAttemptTrace(
            &inputTrace, work.binding.dueKind, gameSecond,
            callbackSequence);
        inputTrace.nextRetryGameSecond = work.binding.nextDueGameSecond;
    }
    // Binding/input edges must complete at least one original native callback
    // and the full settle/load gate must still be open before any registry
    // resolution.  A closed gate preserves the due value for the first safe
    // callback instead of manufacturing a game-time retry delay.
    const char* preResolveGate = ProductionDueTransactionGateReason(work);
    if (strcmp(preResolveGate, "open") != 0) {
        if (selectedInputWork) {
            inputTrace.transactionGate = preResolveGate;
            ProductionLogInputNoWrite(
                work.binding, inputTrace, preResolveGate,
                work.binding.state);
        }
        return;
    }

    void* save = nullptr;
    ProductionDueResolution resolution = {};
    bool callbackSaveReady = false;
    bool dueResolved = false;
    {
        NearbyReadableRegionCacheScope snapshotScope(&dueReadableCache);
        callbackSaveReady = ProductionReadCallbackSave(
            worldEpoch, loadGeneration, &save);
        dueResolved = callbackSaveReady &&
            ProductionResolveDueBinding(save, loadGeneration, work.binding,
                                        &resolution, &dueReadableCache);
    }
    dueTiming.resolved = NearbyPerformanceCounter();
    if (!dueResolved) {
        const char* postResolveGate =
            ProductionDueTransactionGateReason(work);
        if (strcmp(postResolveGate, "open") != 0) {
            if (selectedInputWork) {
                inputTrace.transactionGate = postResolveGate;
                ProductionLogInputNoWrite(
                    work.binding, inputTrace, postResolveGate,
                    work.binding.state);
            }
            return;
        }
        const u32 retry = ProductionBackoffDueBinding(
            work, ProductionState::Starting,
            ProductionReason::RegistryInvalid, ProductionDueKind::Dirty,
            PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
        if (selectedInputWork) {
            inputTrace.transactionGate = callbackSaveReady
                ? "stable_id_resolve_invalid" : "callback_save_invalid";
            inputTrace.nextRetryGameSecond = ProductionAddGameTicks(
                gameSecond, retry);
            ProductionLogInputNoWrite(
                work.binding, inputTrace, inputTrace.transactionGate,
                ProductionState::Starting);
        } else {
            ProductionLogOutputNoWrite(
                callbackSequence, work.binding, nullptr,
                PRODUCTION_MACHINE_SLOTS, 0, 0, 0, work.binding.dueKind,
                callbackSaveReady ? "stable_id_resolve_invalid" :
                                    "callback_save_invalid",
                "output_resolve_invalid", gameSecond,
                ProductionAddGameTicks(gameSecond, retry),
                ProductionState::Starting);
        }
        return;
    }

    ProductionBinding evaluated = work.binding;
    if (resolution.ambiguous) {
        evaluated.state = ProductionState::Faulted;
        evaluated.reason = ProductionReason::AmbiguousIdentity;
    } else {
        ProductionEvaluateBinding(&evaluated, resolution.objects,
                                  resolution.objectCount, gameSecond);
    }
    const ProductionObjectSnapshot* machine = ProductionFindObject(
        resolution.objects, resolution.objectCount, work.binding.machine);
    ProductionBinding candidate = {};
    if (!ProductionCommitDueEvaluation(
            work, evaluated, machine, gameSecond, &candidate)) {
        if (selectedInputWork) {
            inputTrace.transactionGate = "evaluation_revision_changed";
            inputTrace.nextRetryGameSecond = work.binding.nextDueGameSecond;
            ProductionLogInputNoWrite(
                work.binding, inputTrace, "evaluation_revision_changed",
                work.binding.state);
        }
        return;
    }

    const ProductionStatusHit* machineHit = ProductionFindStatusHit(
        resolution.hits, resolution.hitCount, candidate.machine);
    if (candidate.state == ProductionState::OutputReady) {
        const ProductionStatusHit* outputHit = ProductionFindStatusHit(
            resolution.hits, resolution.hitCount, candidate.output);
        if (!machineHit || !outputHit || !machine) {
            const u32 retry = ProductionBackoffDueBinding(
                work, candidate.state, candidate.reason,
                ProductionDueKind::Dirty,
                PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
            ProductionLogOutputNoWrite(
                callbackSequence, candidate, machine,
                PRODUCTION_MACHINE_SLOTS, 0, 0, 0, candidate.dueKind,
                !machineHit ? "machine_status_unresolved" :
                (!outputHit ? "output_status_unresolved" :
                              "machine_snapshot_missing"),
                "output_resolve_invalid", gameSecond,
                ProductionAddGameTicks(gameSecond, retry),
                candidate.state);
            return;
        }
        if (!PRODUCTION_NATIVE_TRANSACTIONS_PROVEN) {
            ProductionBackoffDueBinding(
                work, ProductionState::Faulted,
                ProductionReason::NativeTransactionEvidenceRequired,
                ProductionDueKind::Parked,
                PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
            ProductionLogOutputNoWrite(
                callbackSequence, candidate, machine,
                PRODUCTION_MACHINE_SLOTS, 0, 0, 0, candidate.dueKind,
                "native_transaction_unproven",
                "native_transaction_unproven", gameSecond,
                INT64_MAX, ProductionState::Faulted);
            return;
        }
        const char* outputReservationGate = "not_attempted";
        if (!ProductionReserveDueTransaction(
                work, candidate, false, &outputReservationGate)) {
            const char* outputGateDetail =
                ProductionDueTransactionGateReason(work);
            if (strcmp(outputGateDetail, "open") != 0) return;
            const u32 retry = ProductionBackoffDueBinding(
                work, candidate.state, candidate.reason,
                ProductionDueKind::Dirty,
                PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
            ProductionLogOutputNoWrite(
                callbackSequence, candidate, machine,
                PRODUCTION_MACHINE_SLOTS, 0, 0, 0, candidate.dueKind,
                outputReservationGate,
                outputReservationGate, gameSecond,
                ProductionAddGameTicks(gameSecond, retry),
                candidate.state);
            return;
        }
        bool transferred = false;
        bool capacityWait = false;
        bool mutationAttempted = false;
        const char* outputFailReason = nullptr;
        ProductionState stateAfter = ProductionState::OutputReady;
        ProductionReason reasonAfter = ProductionReason::OutputNeedsTransfer;
        {
            NearbyReadableRegionCacheScope transactionScope(
                &dueReadableCache);
            ProductionAttemptOutputTransfer(
                machineHit->status, outputHit->status, candidate, machine,
                callbackSequence, gameSecond, &transferred, &capacityWait,
                &mutationAttempted, &outputFailReason);
            if (transferred && !ProductionClassifyMachineAfterTransfer(
                    machineHit->status, gameSecond, &stateAfter,
                    &reasonAfter)) {
                stateAfter = ProductionState::Faulted;
                reasonAfter = ProductionReason::RegistryInvalid;
                ProductionFaultTransfer("post_transfer_state_unreadable",
                                        nullptr);
            }
        }
        if (mutationAttempted)
            ProductionArmPostTransactionGate(callbackSequence);
        g_productionTransferInFlight.store(false,
                                            std::memory_order_release);
        dueTiming.attempted = NearbyPerformanceCounter();
        if (transferred) {
            ProductionObjectSnapshot after = {};
            const ProductionObjectSnapshot* scheduleMachine = nullptr;
            {
                NearbyReadableRegionCacheScope readbackScope(
                    &dueReadableCache);
                if (ProductionReadObject(machineHit->status, &after, false,
                                         false))
                    scheduleMachine = &after;
            }
            ProductionUpdateDueBindingAfterAttempt(
                work, stateAfter, reasonAfter, scheduleMachine, gameSecond,
                callbackSequence);
        } else if (capacityWait) {
            // Before backing off, try rotating to the next output chest in
            // the flood-fill group.  If rotation succeeds the binding becomes
            // Dirty and will retry delivery on the next callback.
            bool rotated = false;
            {
                AcquireSRWLockExclusive(&g_productionBindingLock);
                ProductionBinding* live = ProductionFindBindingLocked(
                    candidate.machine, false);
            if (live && live->autoLinked &&
                live->nearestChestCount > 0) {
                rotated = ProductionRotateOutputChest(live, gameSecond);
                }
                ReleaseSRWLockExclusive(&g_productionBindingLock);
            }
            if (rotated) {
                ProductionLogOutputNoWrite(
                    callbackSequence, candidate, machine,
                    PRODUCTION_MACHINE_SLOTS, 0, 0, 0, candidate.dueKind,
                    "output_rotated_to_next_chest",
                    "output_rotated_to_next_chest", gameSecond,
                    0, candidate.state);
                return;
            }
            ProductionBackoffDueBinding(
                work, ProductionState::WaitingOutputSpace,
                ProductionReason::OutputCapacityInsufficient,
                ProductionDueKind::OutputRetry,
                PRODUCTION_OUTPUT_RETRY_GAME_TICKS, gameSecond);
        } else if (g_productionTransferFaulted.load(
                       std::memory_order_acquire)) {
            ProductionBackoffDueBinding(
                work, ProductionState::Faulted,
                ProductionReason::RegistryInvalid,
                ProductionDueKind::Parked,
                PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
            // The offending binding is parked and any quarantined item is
            // retained for process teardown.  Keep the other machines alive:
            // a per-machine native verification failure must never disable
            // every production binding for the rest of the session.
            g_productionTransferFaulted.store(false, std::memory_order_release);
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=transfer_fault_released "
                "result=PASS reason=faulted_binding_parked "
                "device_map_id=%llu device_id=%llu "
                "raw_pointer_cache=0 writes=0\n",
                static_cast<unsigned long long>(candidate.machine.mapId),
                static_cast<unsigned long long>(candidate.machine.uniqueId));
        } else {
            const char* noWriteGate = outputFailReason
                ? outputFailReason : "output_helper_rejected_without_write";
            const bool transientNoWrite =
                strcmp(noWriteGate, "no_finished_slot") == 0 ||
                strcmp(noWriteGate, "machine_status_invalid") == 0 ||
                strcmp(noWriteGate, "output_identity_mismatch") == 0 ||
                strcmp(noWriteGate, "inproc_probe_unavailable") == 0 ||
                strcmp(noWriteGate, "inproc_reset_unavailable") == 0;
            const u32 retry = ProductionBackoffDueBinding(
                work, candidate.state, candidate.reason,
                transientNoWrite ? ProductionDueKind::OutputRetry :
                                   ProductionDueKind::Dirty,
                transientNoWrite
                    ? PRODUCTION_TRANSIENT_OUTPUT_RETRY_GAME_TICKS
                    : PRODUCTION_RESOLVE_RETRY_GAME_TICKS,
                gameSecond);
            ProductionLogOutputNoWrite(
                callbackSequence, candidate, machine,
                PRODUCTION_MACHINE_SLOTS, 0, 0, 0, candidate.dueKind,
                noWriteGate, noWriteGate, gameSecond,
                ProductionAddGameTicks(gameSecond, retry),
                candidate.state);
        }
        return;
    }

    // Dirty/finish/output-retry work is selected in the output-priority pass.
    // If its fresh snapshot is merely idle, commit the InputRetry schedule but
    // do not spend this callback on input: another binding may still have a
    // real finished product.  The next callback reaches the input pass only
    // after every currently-due output-class binding has had its turn.
    const bool runningSlotRefill =
        candidate.state == ProductionState::Running &&
        candidate.dueKind == ProductionDueKind::InputRetry;
    if ((candidate.state != ProductionState::WaitingInput &&
         !runningSlotRefill) ||
        work.outputPriority) {
        if (selectedInputWork) {
            inputTrace.transactionGate = "machine_state_changed";
            inputTrace.nextRetryGameSecond = candidate.nextDueGameSecond;
            ProductionLogInputNoWrite(
                candidate, inputTrace, "machine_state_changed",
                candidate.state);
        }
        return;
    }
    if (runningSlotRefill &&
        (!machine || !ProductionMachineHasIdleInputSlot(machine))) {
        // The previously idle slot was claimed by native state (or the whole
        // machine matured) between the schedule snapshot and this callback.
        // Re-evaluate on the next safe callback; never manufacture a cooldown.
        ProductionBackoffDueBinding(
            work, candidate.state, candidate.reason,
            ProductionDueKind::Dirty, 1, gameSecond);
        if (selectedInputWork) {
            inputTrace.transactionGate = "running_slot_refill_snapshot_changed";
            inputTrace.nextRetryGameSecond = ProductionAddGameTicks(
                gameSecond, 1);
            ProductionLogInputNoWrite(
                candidate, inputTrace, "running_slot_refill_snapshot_changed",
                candidate.state);
        }
        return;
    }
    const ProductionStatusHit* inputHit = ProductionFindStatusHit(
        resolution.hits, resolution.hitCount, candidate.input);
    if (!machineHit || !inputHit || !machine) {
        const u32 retry = ProductionBackoffDueBinding(
            work, candidate.state, candidate.reason,
            ProductionDueKind::Dirty,
            PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
        inputTrace.transactionGate = !machineHit
            ? "machine_status_unresolved" :
            (!inputHit ? "input_status_unresolved" :
                         "machine_snapshot_missing");
        inputTrace.nextRetryGameSecond = ProductionAddGameTicks(
            gameSecond, retry);
        ProductionLogInputNoWrite(
            candidate, inputTrace, inputTrace.transactionGate,
            candidate.state);
        return;
    }
    if (!PRODUCTION_NATIVE_TRANSACTIONS_PROVEN) {
        ProductionBackoffDueBinding(
            work, ProductionState::Faulted,
            ProductionReason::NativeTransactionEvidenceRequired,
            ProductionDueKind::Parked,
            PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
        inputTrace.transactionGate = "native_transaction_unproven";
        inputTrace.nextRetryGameSecond = INT64_MAX;
        ProductionLogInputNoWrite(
            candidate, inputTrace, "native_transaction_unproven",
            ProductionState::Faulted);
        return;
    }
    const char* reservationGate = "not_attempted";
    if (!ProductionReserveDueTransaction(
            work, candidate, true, &reservationGate)) {
        inputTrace.reservationGate = reservationGate;
        inputTrace.transactionGate = ProductionDueTransactionGateReason(work);
        if (strcmp(inputTrace.transactionGate, "open") != 0) {
            inputTrace.nextRetryGameSecond = candidate.nextDueGameSecond;
            ProductionLogInputNoWrite(
                candidate, inputTrace, reservationGate, candidate.state);
            return;
        }
        const u32 retry = ProductionBackoffDueBinding(
            work, candidate.state, candidate.reason,
            ProductionDueKind::Dirty,
            PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
        inputTrace.nextRetryGameSecond = ProductionAddGameTicks(
            gameSecond, retry);
        ProductionLogInputNoWrite(
            candidate, inputTrace, reservationGate, candidate.state);
        return;
    }
    inputTrace.reservationGate = reservationGate;
    bool started = false;
    bool mutationAttempted = false;
    std::int64_t expectedFinish = 0;
    ProductionInputAttemptResult result =
        ProductionInputAttemptResult::ResolveInvalid;
    {
        NearbyReadableRegionCacheScope transactionScope(&dueReadableCache);
        result = ProductionAttemptInputTransfer(
            save, machineHit->status, inputHit->status, work, candidate,
            machine, gameSecond, &started, &mutationAttempted,
            &expectedFinish, &inputTrace);
    }
    if (mutationAttempted)
        ProductionArmPostTransactionGate(callbackSequence);
    g_productionTransferInFlight.store(false, std::memory_order_release);
    dueTiming.attempted = NearbyPerformanceCounter();
    if (result == ProductionInputAttemptResult::Started && started) {
        ProductionUpdateDueBindingAfterAttempt(
            work, ProductionState::Running,
            ProductionReason::RunningNativeTimer, nullptr, gameSecond,
            callbackSequence,
            expectedFinish);
        // Ingredients were consumed; a stale fingerprint must not wake this
        // binding when the machine is now running.
        {
            AcquireSRWLockExclusive(&g_productionBindingLock);
            ProductionBinding* live = ProductionFindBindingLocked(
                candidate.machine, false);
            if (live) live->inputFingerprint = 0;
            ReleaseSRWLockExclusive(&g_productionBindingLock);
        }
    } else if (result ==
               ProductionInputAttemptResult::NoSatisfiableRecipe) {
        // Before backing off, try rotating to the next chest in the
        // flood-fill group.  If rotation succeeds the binding becomes Dirty
        // and will be re-evaluated on the next callback with a fresh chest.
        bool rotated = false;
        {
            AcquireSRWLockExclusive(&g_productionBindingLock);
            ProductionBinding* live = ProductionFindBindingLocked(
                candidate.machine, false);
            if (live && live->autoLinked &&
                live->nearestChestCount > 0) {
                rotated = ProductionRotateInputChest(live, gameSecond);
            }
            ReleaseSRWLockExclusive(&g_productionBindingLock);
        }
        if (rotated) {
            ProductionLogInputNoWrite(
                candidate, inputTrace, "input_rotated_to_next_chest",
                candidate.state);
            return;
        }
        const bool runningRefill =
            candidate.state == ProductionState::Running &&
            candidate.dueKind == ProductionDueKind::InputRetry;
        const u32 retry = ProductionBackoffDueBinding(
            work, runningRefill ? ProductionState::Running :
                                  ProductionState::WaitingInput,
            ProductionReason::WaitingIngredients,
            ProductionDueKind::InputRetry,
            PRODUCTION_INPUT_RETRY_GAME_TICKS, gameSecond);
        inputTrace.nextRetryGameSecond = ProductionAddGameTicks(
            gameSecond, retry);
        // Record the current input chest fingerprint so a later scan can
        // detect the player added ingredients and wake this binding early.
        {
            AcquireSRWLockExclusive(&g_productionBindingLock);
            ProductionBinding* live = ProductionFindBindingLocked(
                candidate.machine, false);
            if (live && live->hasInput) {
                live->inputFingerprint =
                    ProductionChestFingerprint(inputHit->status);
            }
            ReleaseSRWLockExclusive(&g_productionBindingLock);
        }
        ProductionLogInputNoWrite(
            candidate, inputTrace, "no_satisfiable_recipe",
            runningRefill ? ProductionState::Running :
                            ProductionState::WaitingInput);
    } else if (result == ProductionInputAttemptResult::MachineNotIdle ||
               result == ProductionInputAttemptResult::ResolveInvalid) {
        const bool runningRefill =
            candidate.state == ProductionState::Running &&
            candidate.dueKind == ProductionDueKind::InputRetry;
        // A WaitingInput machine that failed a callback-local validation gets
        // a two-game-minute InputRetry instead of an hour-long Dirty stall.
        const bool waitingShortRetry =
            !runningRefill &&
            candidate.state == ProductionState::WaitingInput;
        const u32 retry = ProductionBackoffDueBinding(
            work, candidate.state, candidate.reason,
            runningRefill ? ProductionDueKind::Dirty :
            (waitingShortRetry ? ProductionDueKind::InputRetry :
                                 ProductionDueKind::Dirty),
            runningRefill ? 1 :
            (waitingShortRetry
                 ? PRODUCTION_TRANSIENT_INPUT_RETRY_GAME_TICKS
                 : PRODUCTION_RESOLVE_RETRY_GAME_TICKS),
            gameSecond);
        const char* reason = result ==
                ProductionInputAttemptResult::MachineNotIdle
            ? "machine_not_idle" : "input_resolve_invalid";
        inputTrace.nextRetryGameSecond = ProductionAddGameTicks(
            gameSecond, retry);
        ProductionLogInputNoWrite(
            candidate, inputTrace, reason, candidate.state);
    } else if (result == ProductionInputAttemptResult::Faulted) {
        ProductionBackoffDueBinding(
            work, ProductionState::Faulted,
            ProductionReason::RegistryInvalid,
            ProductionDueKind::Parked,
            PRODUCTION_RESOLVE_RETRY_GAME_TICKS, gameSecond);
        g_productionTransferFaulted.store(false, std::memory_order_release);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=transfer_fault_released "
            "result=PASS reason=faulted_binding_parked "
            "device_map_id=%llu device_id=%llu "
            "raw_pointer_cache=0 writes=0\n",
            static_cast<unsigned long long>(candidate.machine.mapId),
            static_cast<unsigned long long>(candidate.machine.uniqueId));
    }
}

static void ProductionPublishScanProgress(bool active, size_t processed,

                                          size_t total,
                                          bool invalidateObjects = false) {
    AcquireSRWLockExclusive(&g_productionPublishLock);
    if (invalidateObjects) {
        g_productionPublished.generation =
            g_productionSequence.fetch_add(1, std::memory_order_relaxed) + 1;
        g_productionPublished.chestCount = 0;
    }
    g_productionPublished.scanActive = active;
    g_productionPublished.scanProcessed = processed;
    g_productionPublished.scanTotal = total;
    ReleaseSRWLockExclusive(&g_productionPublishLock);
}

static void ProductionPublishPendingMachine(
        const ProductionStableId& active, ProductionState state,
        ProductionReason reason) {
    AcquireSRWLockExclusive(&g_productionPublishLock);
    g_productionPublished.generation =
        g_productionSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    g_productionPublished.activeMachine = active;
    g_productionPublished.activeMachinePresent =
        active.mapId != 0 && active.uniqueId != 0;
    g_productionPublished.activeState = state;
    g_productionPublished.activeReason = reason;
    g_productionPublished.scanActive = true;
    g_productionPublished.scanProcessed = 0;
    g_productionPublished.scanTotal = 0;
    g_productionPublished.chestCount = 0;
    ReleaseSRWLockExclusive(&g_productionPublishLock);
}

static void ProductionEndScanAttempt(size_t processed, size_t total,
                                     bool invalidateObjects) {
    g_productionScanActive = false;
    g_productionScanning = false;
    g_productionScanResumeNode = nullptr;
    g_productionScanResumePrev = nullptr;
    g_productionScanInProgress.store(false, std::memory_order_release);
    g_productionScanOwnerThread.store(0, std::memory_order_release);
    ProductionPublishScanProgress(false, processed, total, invalidateObjects);
    // A completed/aborted full scan requires a quiet native window before a
    // due transaction; there is no periodic registry timer to reset.
    ProductionArmTransferCooldown(PRODUCTION_POST_SCAN_SETTLE_MS);
}

static void ProductionOnMainThreadPreUpdate(u64 callbackSequence,
                                            bool callbackWasInterrupted) {
    if (callbackSequence == 0 || g_productionMainCallbackDepth != 1)
        return;
    g_productionPreUpdateStage.store(0, std::memory_order_release);
    if (
        !ProductionFeatureIsEnabled() ||
        !g_productionReady.load(std::memory_order_acquire) ||
        g_productionFaulted.load(std::memory_order_acquire) ||
        !g_productionWorldContextActive.load(std::memory_order_acquire))
        return;

    // Pure binding values are primed on config/world/load edges even while a
    // settle/notBefore/scan gate is closed.  The first safe later callback
    // still resolves at most one selected binding.
    ProductionRefreshSchedulerEpochs();
    // A sleep/day/time-skip edge is published only after the previous native
    // update returned from CGameTime::advance.  Consume the scalar edge here,
    // before due selection but after a complete native callback; this remains
    // silent and performs no root/save/registry/page-protection read.
    ProductionRefreshSchedulerNativeClockWake();
    // A callback gap is not a world/map loss and does not reset/dirty the
    // scheduler.  Existing mutation/load epochs already enforce their own
    // complete-native-callback gate; a resumed steady world may proceed.
    (void)callbackWasInterrupted;

    if (g_productionWorkerTimeoutObserved.exchange(
            false, std::memory_order_acq_rel)) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=transfer_deferred "
            "result=INCOMPLETE reason=worker_timeout_observed "
            "callback_sequence=%llu cooldown_ms=0 transaction_cooldown=0 "
            "raw_pointer_cache=0 writes=0\n",
            static_cast<unsigned long long>(callbackSequence));
    }

    if (g_productionRegistryScanRequested.load(std::memory_order_acquire) ||
        g_productionScanInProgress.load(std::memory_order_acquire) ||
        g_productionScanActive || g_productionScanning) {
        ProductionArmTransferCooldown(PRODUCTION_POST_SCAN_SETTLE_MS);
    }

    // No wall-clock poll and no all-binding registry walk.  The scheduler
    // reads only gameSecond and binding values until one binding is due.
    // ProductionRunDueStateMachine owns its own throttled QPC slow-path log.
    // A burst is drained with a bounded budget: at most a few transactions in
    // this callback, never an unbounded morning freeze.
    const LONGLONG batchBegin = NearbyPerformanceCounter();
    for (unsigned batchPass = 0;
         batchPass < PRODUCTION_DUE_BATCH_MAX_PER_CALLBACK; ++batchPass) {
        if (!ProductionHasScheduledBinding()) break;
        if (ProductionTicksToMicroseconds(
                NearbyPerformanceCounter() - batchBegin) >
            PRODUCTION_DUE_BATCH_BUDGET_US)
            break;
        ProductionRunDueStateMachine(callbackSequence);
    }
    g_productionPreUpdateStage.store(0, std::memory_order_release);
}

// ---- Chain automation: flood-fill position-driven auto-binding -------------
//
// After each scan completes, all machines and chests are flood-filled into
// connected groups based on XY-plane adjacency within PRODUCTION_AUTOLINK_RADIUS.
// Machines can be adjacent to chests, machines to machines, and chests to chests,
// forming arbitrary connected groups (like gloaming's Automate).
//
// A machine in a group takes input from ANY chest in the group and delivers
// output to ANY chest in the group.  When the current chest fails (no recipe /
// full), the binding rotates to the next chest in the group on the next cycle.
//
//   Group with 1 chest  =>  input = output = that chest (single-chest mode)
//   Group with 2+ chests =>  nearest chest = input, second nearest = output
//                            (rotation tries others on failure)
//
// Auto-linked bindings carry autoLinked=true, which relaxes the SameChest and
// unnamed-chest guards in ProductionEvaluateBinding.
//
// This function must be called on the main thread with g_productionBindingLock
// held exclusively, AFTER the scan objects are published in g_productionScanObjects.

static void ProductionAutoLinkBindings(
        const ProductionObjectSnapshot* objects, size_t objectCount) {
    if (!objects || objectCount == 0) return;

    size_t linkedCount = 0;
    size_t updatedCount = 0;
    size_t disabledCount = 0;

    // Time budget: this runs inside an exclusive lock on the main thread.
    const LONGLONG budgetBegin = NearbyPerformanceCounter();
    static constexpr LONGLONG PRODUCTION_AUTOLINK_BUDGET_US = 4000;

    // ---- Step 0: Chest settle detection ----
    // A chest seen in a previous scan is immediately settled.
    // A chest seen for the first time (newly placed) must wait
    // PRODUCTION_CHEST_SETTLE_MS before being included.
    // On the very first scan after game load, all chests are settled.
    const ULONGLONG nowTick = GetTickCount64();
    for (size_t si = 0; si < PRODUCTION_MAX_CHESTS; ++si)
        g_productionChestSettle[si].seenThisScan = false;
    for (size_t i = 0; i < objectCount; ++i)
        g_productionChestSettled[i] = false;

    for (size_t i = 0; i < objectCount; ++i) {
        const ProductionObjectSnapshot& o = objects[i];
        if (!o.chest || !o.inventoryValid) continue;

        ProductionChestSettleRecord* rec = nullptr;
        for (size_t si = 0; si < PRODUCTION_MAX_CHESTS; ++si) {
            if (g_productionChestSettle[si].valid &&
                ProductionIdEqual(g_productionChestSettle[si].id, o.id)) {
                rec = &g_productionChestSettle[si];
                break;
            }
        }
        if (rec) {
            // Known chest: settled immediately.
            rec->seenThisScan = true;
            g_productionChestSettled[i] = true;
        } else {
            // New chest: create record, check settle timer.
            for (size_t si = 0; si < PRODUCTION_MAX_CHESTS; ++si) {
                if (!g_productionChestSettle[si].valid) {
                    rec = &g_productionChestSettle[si];
                    rec->valid = true;
                    rec->id = o.id;
                    rec->firstSeenTick = nowTick;
                    rec->seenThisScan = true;
                    break;
                }
            }
            // On first scan (save load), all chests are already settled.
            // After that, new chests must wait the settle period.
            g_productionChestSettled[i] = g_productionChestSettleFirstScan ||
                (nowTick - rec->firstSeenTick) >= PRODUCTION_CHEST_SETTLE_MS;
        }
    }
    g_productionChestSettleFirstScan = false;
    // Invalidate records for chests that disappeared.
    for (size_t si = 0; si < PRODUCTION_MAX_CHESTS; ++si) {
        if (g_productionChestSettle[si].valid &&
            !g_productionChestSettle[si].seenThisScan) {
            g_productionChestSettle[si].valid = false;
        }
    }

    // ---- Step 1: Build adjacency + flood-fill into groups ----
    g_productionChestGroupCount = 0;
    // visited[i] = group index + 1 (0 = unvisited)
    size_t visited[PRODUCTION_MAX_OBJECT_SNAPSHOTS] = {};
    // BFS queue (indices into objects[])
    size_t bfsQueue[PRODUCTION_MAX_OBJECT_SNAPSHOTS] = {};
    size_t bfsHead = 0, bfsTail = 0;

    // Pre-filter: build a compact index of only machine/chest/floor objects
    // so the BFS inner loop skips non-relevant entries (decorations, etc.).
    size_t activeIndices[PRODUCTION_MAX_OBJECT_SNAPSHOTS] = {};
    size_t activeCount = 0;
    for (size_t i = 0; i < objectCount; ++i) {
        if (objects[i].machine || objects[i].chest || objects[i].floor)
            activeIndices[activeCount++] = i;
    }

    for (size_t seed = 0; seed < objectCount; ++seed) {
        if (visited[seed]) continue;
        if (!objects[seed].machine && !objects[seed].chest) continue;
        // Floor tiles are bridge nodes, not seeds. They are discovered
        // during BFS expansion from a machine or chest.
        if (objects[seed].floor) continue;
        // Skip unsettled chests (player is moving them).
        if (objects[seed].chest && !g_productionChestSettled[seed]) {
            visited[seed] = SIZE_MAX;
            continue;
        }

        const size_t groupIdx = g_productionChestGroupCount;
        if (groupIdx >= PRODUCTION_MAX_GROUPS) break;
        ProductionChestGroup& group = g_productionChestGroupCount == groupIdx
            ? g_productionChestGroups[groupIdx]
            : g_productionChestGroups[g_productionChestGroupCount];
        group.chestCount = 0;
        group.machineCount = 0;

        bfsHead = 0;
        bfsTail = 0;
        bfsQueue[bfsTail++] = seed;
        visited[seed] = groupIdx + 1;

        while (bfsHead < bfsTail) {
            if (ProductionTicksToMicroseconds(
                    NearbyPerformanceCounter() - budgetBegin) >
                PRODUCTION_AUTOLINK_BUDGET_US) {
                ProductionLog(
                    "[PRODAUTO] seq=%llu txn=0 event=autolink_timeout "
                    "group=%zu machines=%zu chests=%zu\n",
                    static_cast<unsigned long long>(
                        g_productionSequence.load(std::memory_order_relaxed)),
                    groupIdx, group.machineCount, group.chestCount);
                break;
            }

            const size_t cur = bfsQueue[bfsHead++];
            const ProductionObjectSnapshot& curObj = objects[cur];

            // Add to group (only machines and chests, never floors).
            if (curObj.floor) {
                // Floor is a bridge node: it connects objects on either
                // side but is not itself a producer or container.
            } else if (curObj.machine && group.machineCount <
                    PRODUCTION_MAX_MACHINES_PER_GROUP) {
                group.machines[group.machineCount++] = curObj.id;
            } else if (curObj.chest && curObj.inventoryValid &&
                g_productionChestSettled[cur] &&
                group.chestCount < PRODUCTION_MAX_CHESTS_PER_GROUP) {
                group.chests[group.chestCount++] = curObj.id;
            }

            // Find all adjacent objects (machines, chests, OR floors).
            for (size_t ai = 0; ai < activeCount; ++ai) {
                const size_t ni = activeIndices[ai];
                if (visited[ni]) continue;
                // Accept machines, chests, and floor tiles as neighbors.
                if (!objects[ni].machine && !objects[ni].chest &&
                    !objects[ni].floor) continue;
                // Floor tiles are always eligible as bridge nodes.
                if (!objects[ni].floor) {
                    if (!objects[ni].chest || !objects[ni].inventoryValid) {
                        // Allow machine-machine adjacency.
                        if (!objects[ni].machine) continue;
                    }
                    // Skip unsettled chests in adjacency too.
                    if (objects[ni].chest && !g_productionChestSettled[ni]) {
                        visited[ni] = SIZE_MAX;
                        continue;
                    }
                }
                const float dx = objects[ni].position[0] - curObj.position[0];
                const float dy = objects[ni].position[1] - curObj.position[1];
                const float distSq = dx * dx + dy * dy;
                if (distSq > PRODUCTION_AUTOLINK_RADIUS_SQ) continue;
                visited[ni] = groupIdx + 1;
                if (bfsTail < PRODUCTION_MAX_OBJECT_SNAPSHOTS)
                    bfsQueue[bfsTail++] = ni;
            }
        }

        // Only keep groups that have at least one machine and one chest.
        if (group.machineCount > 0 && group.chestCount > 0) {
            ++g_productionChestGroupCount;
        }
    }

    // ---- Step 1.5: Floor bridge diagnostic ----
    // Count floor tiles scanned and how many acted as bridge nodes.
    // Throttled to once per ~60 s to avoid log spam.
    {
        static ULONGLONG s_lastFloorDiag = 0;
        if (nowTick - s_lastFloorDiag >= 60000) {
            s_lastFloorDiag = nowTick;
            size_t floorTotal = 0, floorBridged = 0;
            for (size_t i = 0; i < objectCount; ++i) {
                if (!objects[i].floor) continue;
                ++floorTotal;
                if (visited[i] != 0 && visited[i] != SIZE_MAX)
                    ++floorBridged;
            }
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=floor_diag "
                "floor_total=%zu floor_bridged=%zu groups=%zu\n",
                static_cast<unsigned long long>(
                    g_productionSequence.load(std::memory_order_relaxed)),
                floorTotal, floorBridged,
                g_productionChestGroupCount);
        }
    }

    // ---- Step 1.5: 诊断日志（不改变逻辑）----
    // 输出本次诊断过程中机器/箱子的识别情况，以及哪些对象未进入任何连通组
    // 便于排查"设计进隔壁却没自动连接"的问题
    // 保守版：未进组对象最多打印前 20 个，避免海量日志调用触发格式化开销
#if 0 // diagnostic dead code (was if(false))
    if (false) {
        size_t chestTotal = 0, machineTotal = 0;
        size_t chestInGroup = 0, machineInGroup = 0;
        size_t ungroupedMachinesLogged = 0, ungroupedChestsLogged = 0;
        constexpr size_t kMaxUngroupedMachines = 80;
        constexpr size_t kMaxUngroupedChests = 64;
        // 先遍历统计是否进组
        for (size_t i = 0; i < objectCount; ++i) {
            const ProductionObjectSnapshot& o = objects[i];
            if (o.chest) ++chestTotal;
            if (o.machine) ++machineTotal;
            bool inGroup = false;
            for (size_t gi = 0; gi < g_productionChestGroupCount && !inGroup;
                 ++gi) {
                const ProductionChestGroup& g = g_productionChestGroups[gi];
                for (size_t ci = 0; ci < g.chestCount; ++ci)
                    if (ProductionIdEqual(g.chests[ci], o.id)) { inGroup = true; break; }
                if (!inGroup)
                    for (size_t mi = 0; mi < g.machineCount; ++mi)
                        if (ProductionIdEqual(g.machines[mi], o.id)) { inGroup = true; break; }
            }
            if (inGroup) {
                if (o.chest) ++chestInGroup;
                if (o.machine) ++machineInGroup;
            }
        }
        // 未进组的机器优先打印（蜂巢/蚕盒等加工机最需要看到）
        for (size_t i = 0; i < objectCount; ++i) {
                const ProductionObjectSnapshot& o = objects[i];
                if (!o.machine) continue;
                bool inGroup = false;
                for (size_t gi = 0; gi < g_productionChestGroupCount && !inGroup;
                     ++gi) {
                    const ProductionChestGroup& g = g_productionChestGroups[gi];
                    for (size_t ci = 0; ci < g.chestCount; ++ci)
                        if (ProductionIdEqual(g.chests[ci], o.id)) { inGroup = true; break; }
                    if (!inGroup)
                        for (size_t mi = 0; mi < g.machineCount; ++mi)
                            if (ProductionIdEqual(g.machines[mi], o.id)) { inGroup = true; break; }
                }
                if (inGroup) continue;
                if (ungroupedMachinesLogged >= kMaxUngroupedMachines) continue;
                ++ungroupedMachinesLogged;
                // 未进组的机器：打印最近邻居距离（区分机器/箱子）
                float nearestMachineDistSq = 1e30f;
                float nearestChestDistSq = 1e30f;
                for (size_t j = 0; j < objectCount; ++j) {
                    if (i == j) continue;
                    if (!objects[j].machine && !objects[j].chest) continue;
                    const float dx = objects[j].position[0] - o.position[0];
                    const float dy = objects[j].position[1] - o.position[1];
                    const float distSq = dx * dx + dy * dy;
                    if (objects[j].machine && distSq < nearestMachineDistSq)
                        nearestMachineDistSq = distSq;
                    if (objects[j].chest && distSq < nearestChestDistSq)
                        nearestChestDistSq = distSq;
                }
                ProductionLog(
                    "[PRODAUTO] seq=%llu txn=0 event=autolink_diag "
                    "result=UNGROUPED_MACHINE map=%llu id=%llu module=%s "
                    "named=%d machine=%d inv=%d pos=(%.1f,%.1f) "
                    "nearest_machine_d=%.1f nearest_chest_d=%.1f "
                    "radius=%.1f\n",
                    static_cast<unsigned long long>(o.id.mapId),
                    static_cast<unsigned long long>(o.id.uniqueId),
                    o.module[0] ? o.module : "(?)",
                    o.named ? 1 : 0, o.machine ? 1 : 0,
                    o.inventoryValid ? 1 : 0,
                    static_cast<double>(o.position[0]),
                    static_cast<double>(o.position[1]),
                    static_cast<double>(sqrtf(nearestMachineDistSq)),
                    static_cast<double>(sqrtf(nearestChestDistSq)),
                    static_cast<double>(PRODUCTION_AUTOLINK_RADIUS));
        }
        // 未进组的箱子（只打印少量，避免刷屏）
        for (size_t i = 0; i < objectCount; ++i) {
            const ProductionObjectSnapshot& o = objects[i];
            if (!o.chest) continue;
            bool inGroup = false;
            for (size_t gi = 0; gi < g_productionChestGroupCount && !inGroup;
                 ++gi) {
                const ProductionChestGroup& g = g_productionChestGroups[gi];
                for (size_t ci = 0; ci < g.chestCount; ++ci)
                    if (ProductionIdEqual(g.chests[ci], o.id)) { inGroup = true; break; }
            }
            if (inGroup) continue;
            if (ungroupedChestsLogged >= kMaxUngroupedChests) continue;
            ++ungroupedChestsLogged;
            size_t nearCount = 0;
            float nearestMachineDistSq = 1e30f;
            for (size_t j = 0; j < objectCount; ++j) {
                if (i == j) continue;
                if (!objects[j].machine && !objects[j].chest) continue;
                const float dx = objects[j].position[0] - o.position[0];
                const float dy = objects[j].position[1] - o.position[1];
                if (dx * dx + dy * dy <= PRODUCTION_AUTOLINK_RADIUS_SQ)
                    ++nearCount;
                if (objects[j].machine && dx * dx + dy * dy < nearestMachineDistSq)
                    nearestMachineDistSq = dx * dx + dy * dy;
            }
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=autolink_diag "
                "result=UNGROUPED_CHEST map=%llu id=%llu module=%s named=%d "
                "chest=%d machine=%d inv=%d settled=%d pos=(%.1f,%.1f) "
                "near=%zu nearest_machine_d=%.1f\n",
                static_cast<unsigned long long>(o.id.mapId),
                static_cast<unsigned long long>(o.id.uniqueId),
                o.module[0] ? o.module : "(?)",
                o.named ? 1 : 0, o.chest ? 1 : 0, o.machine ? 1 : 0,
                o.inventoryValid ? 1 : 0,
                g_productionChestSettled[i] ? 1 : 0,
                static_cast<double>(o.position[0]),
                static_cast<double>(o.position[1]),
                nearCount,
                static_cast<double>(sqrtf(nearestMachineDistSq)));
        }
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 diag=autolink_stats "
            "chest_total=%zu machine_total=%zu chest_in_group=%zu "
            "machine_in_group=%zu groups=%zu ungrouped_machines_logged=%zu "
            "ungrouped_chests_logged=%zu\n",
            chestTotal, machineTotal, chestInGroup, machineInGroup,
            g_productionChestGroupCount, ungroupedMachinesLogged,
            ungroupedChestsLogged);
    }
#endif // diagnostic dead code (was if(false))

    // ---- Step 2: For each machine in each group, create/update bindings ----
    for (size_t gi = 0; gi < g_productionChestGroupCount; ++gi) {
        if (ProductionTicksToMicroseconds(
                NearbyPerformanceCounter() - budgetBegin) >
            PRODUCTION_AUTOLINK_BUDGET_US) break;

        const ProductionChestGroup& group = g_productionChestGroups[gi];

        for (size_t mi = 0; mi < group.machineCount; ++mi) {
            const ProductionStableId& machineId = group.machines[mi];
            ProductionBinding* binding =
                ProductionFindBindingLocked(machineId, true);
            if (!binding) continue;

            if (group.chestCount == 0) {
                if (binding->autoLinked && binding->enabled) {
                    binding->enabled = false;
                    binding->hasInput = false;
                    binding->hasOutput = false;
                    binding->groupIndex = SIZE_MAX;
                    ProductionParkBinding(binding);
                    ++disabledCount;
                }
                continue;
            }

            // Find machine position for distance sorting.
            const ProductionObjectSnapshot* machineObj = nullptr;
            for (size_t oi = 0; oi < objectCount; ++oi) {
                if (ProductionIdEqual(objects[oi].id, machineId)) {
                    machineObj = &objects[oi];
                    break;
                }
            }
            if (!machineObj) continue;

            // Determine if sorting is needed: only when the binding
            // is new, the group changed, or the nearest chest set changed.
            // In steady state (autoLinked, enabled, committed, same group),
            // the sort is skipped entirely to avoid O(chests^2 * objects) work.
            const uint8_t nearestCount = static_cast<uint8_t>(
                group.chestCount < PRODUCTION_NEAREST_CHESTS_PER_BINDING
                    ? group.chestCount
                    : PRODUCTION_NEAREST_CHESTS_PER_BINDING);

            // Quick check: if binding is already auto-linked and stable,
            // compare existing nearestChestIds against group.chests directly.
            // If they match, skip the expensive distance sort entirely.
            bool needSort = !binding->autoLinked;
            if (!needSort) {
                needSort = binding->nearestChestCount != nearestCount;
                for (uint8_t nc = 0; nc < nearestCount && !needSort; ++nc) {
                    if (!ProductionIdEqual(binding->nearestChestIds[nc],
                                           group.chests[nc])) {
                        needSort = true;
                    }
                }
            }

            // Sort group chests by distance to machine (insertion sort, n <= 16).
            ProductionStableId sortedChests[PRODUCTION_MAX_CHESTS_PER_GROUP];
            if (needSort) {
                for (size_t ci = 0; ci < group.chestCount; ++ci)
                    sortedChests[ci] = group.chests[ci];
                for (size_t i = 1; i < group.chestCount; ++i) {
                    ProductionStableId tmp = sortedChests[i];
                    float tmpDistSq = 1e30f;
                    for (size_t oi = 0; oi < objectCount; ++oi) {
                        if (ProductionIdEqual(objects[oi].id, tmp)) {
                            const float dx = objects[oi].position[0] - machineObj->position[0];
                            const float dy = objects[oi].position[1] - machineObj->position[1];
                            tmpDistSq = dx * dx + dy * dy;
                            break;
                        }
                    }
                    size_t j = i;
                    while (j > 0) {
                        float prevDistSq = 1e30f;
                        for (size_t oi = 0; oi < objectCount; ++oi) {
                            if (ProductionIdEqual(objects[oi].id, sortedChests[j - 1])) {
                                const float dx = objects[oi].position[0] - machineObj->position[0];
                                const float dy = objects[oi].position[1] - machineObj->position[1];
                                prevDistSq = dx * dx + dy * dy;
                                break;
                            }
                        }
                        if (prevDistSq <= tmpDistSq) break;
                        sortedChests[j] = sortedChests[j - 1];
                        --j;
                    }
                    sortedChests[j] = tmp;
                }
            } else {
                // Steady state: reuse existing nearest chests as sorted order.
                for (size_t ci = 0; ci < group.chestCount; ++ci)
                    sortedChests[ci] = group.chests[ci];
            }

            // Determine input/output chest IDs.
            ProductionStableId inputId = sortedChests[0];
            ProductionStableId outputId = (group.chestCount >= 2)
                ? sortedChests[1] : sortedChests[0];

            // Check if the nearest chest set changed (after potential sort).
            bool nearestChanged = binding->nearestChestCount != nearestCount;
            for (uint8_t nc = 0; nc < nearestCount && !nearestChanged; ++nc) {
                if (!ProductionIdEqual(binding->nearestChestIds[nc],
                                       sortedChests[nc])) {
                    nearestChanged = true;
                }
            }
            if (nearestChanged) {
                for (uint8_t nc = 0; nc < nearestCount; ++nc)
                    binding->nearestChestIds[nc] = sortedChests[nc];
                binding->nearestChestCount = nearestCount;
                // Nearest set changed: reset rotation state.
                binding->inputRotation = 0;
                binding->outputRotation = 0;
                binding->inputTriedCount = 0;
                binding->outputTriedCount = 0;
            }

            // Update binding.
            const bool wasAutoLinked = binding->autoLinked;
            const bool wasEnabled = binding->enabled;
            const bool wasCommitted = binding->committed;
            const bool wasHasInput = binding->hasInput;
            const bool wasHasOutput = binding->hasOutput;
            const bool groupChanged = binding->groupIndex != gi;

            if (!wasAutoLinked) {
                binding->autoLinked = true;
                binding->input = inputId;
                binding->output = outputId;
                binding->hasInput = true;
                binding->hasOutput = true;
                binding->enabled = true;
                binding->committed = true;
                binding->groupIndex = gi;
                binding->inputRotation = 0;
                binding->outputRotation = 0;
                binding->inputTriedCount = 0;
                binding->outputTriedCount = 0;
                ProductionDirtyBinding(binding);
                g_productionBindingRevision.fetch_add(1, std::memory_order_acq_rel);
                ++linkedCount;
            } else if (!wasEnabled || !wasCommitted ||
                       !wasHasInput || !wasHasOutput || groupChanged) {
                // Binding already exists and may have been rotated by
                // ProductionRotateInputChest/OutputChest.  Do NOT overwrite
                // binding->input/output here only update group membership
                // and re-enable.  Overwriting would reset the rotation state
                // every 60 s and cause a Dirty re-evaluation burst.
                // If the group changed, the old chests are no longer valid so
                // reset to the distance-sorted defaults.
                if (groupChanged) {
                    binding->input = inputId;
                    binding->output = outputId;
                    binding->inputRotation = 0;
                    binding->outputRotation = 0;
                    binding->inputTriedCount = 0;
                    binding->outputTriedCount = 0;
                }
                binding->hasInput = true;
                binding->hasOutput = true;
                binding->enabled = true;
                if (!binding->committed) binding->committed = true;
                binding->groupIndex = gi;
                ProductionDirtyBinding(binding);
                g_productionBindingRevision.fetch_add(1, std::memory_order_acq_rel);
                ++updatedCount;
            }
        }
    }

    // ---- Step 3: Disable & reap bindings for machines no longer in any group ----
    size_t reapedCount = 0;
    for (size_t bi = 0; bi < g_productionBindingCount; ++bi) {
        ProductionBinding& binding = g_productionBindings[bi];
        if (!binding.autoLinked) continue;

        // Valid group membership: group index in range, and the machine is
        // still listed in that group's machine list.
        bool inValidGroup = false;
        if (binding.groupIndex != SIZE_MAX &&
            binding.groupIndex < g_productionChestGroupCount) {
            const ProductionChestGroup& group =
                g_productionChestGroups[binding.groupIndex];
            for (size_t mi = 0; mi < group.machineCount; ++mi) {
                if (ProductionIdEqual(group.machines[mi], binding.machine)) {
                    inValidGroup = true;
                    break;
                }
            }
        }

        if (inValidGroup) {
            // Group still contains this machine.  Nothing to disable, but
            // make sure a previously-disabled binding (e.g. stale group
            // index) is at least consistent.
            continue;
        }

        // Machine is no longer in any valid group. Disable and clear.
        if (binding.enabled || binding.hasInput || binding.hasOutput ||
            binding.groupIndex != SIZE_MAX || binding.nearestChestCount > 0) {
            binding.enabled = false;
            binding.hasInput = false;
            binding.hasOutput = false;
            binding.groupIndex = SIZE_MAX;
            binding.nearestChestCount = 0;
            memset(binding.nearestChestIds, 0, sizeof(binding.nearestChestIds));
            binding.inputRotation = 0;
            binding.outputRotation = 0;
            binding.inputTriedCount = 0;
            binding.outputTriedCount = 0;
            ProductionParkBinding(&binding);
            ++disabledCount;
        }

        // Reap: physically remove the binding from the array via swap-remove
        // if the machine is absent from the current scan snapshot.  This
        // prevents identity drift (uniqueId change) from accumulating stale
        // entries that inflate totalBindings.  Skip if the scan was truncated
        // (we might be missing the machine due to capacity, not removal).
        if (!g_productionScanObjectTruncated && !binding.enabled && !binding.hasInput &&
            !binding.hasOutput && binding.groupIndex == SIZE_MAX) {
            bool foundInScan = false;
            for (size_t oi = 0; oi < objectCount; ++oi) {
                if (ProductionIdEqual(objects[oi].id, binding.machine)) {
                    foundInScan = true;
                    break;
                }
            }
            if (!foundInScan) {
                // Machine disappeared from scan entirely: safe to reap.
                // Swap-remove: move last binding into this slot.
                if (bi != g_productionBindingCount - 1) {
                    g_productionBindings[bi] =
                        g_productionBindings[g_productionBindingCount - 1];
                }
                g_productionBindings[g_productionBindingCount - 1] = {};
                --g_productionBindingCount;
                g_productionBindingRevision.fetch_add(1,
                    std::memory_order_acq_rel);
                ++reapedCount;
                // Re-check this slot (it now holds the swapped-in binding).
                --bi;
            }
        }
    }

    if (linkedCount || updatedCount || disabledCount || reapedCount) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=autolink result=PASS "
                "reason=none linked=%zu updated=%zu disabled=%zu reaped=%zu "
                "total_bindings=%zu groups=%zu writes=0\n",
            linkedCount, updatedCount, disabledCount, reapedCount,
            g_productionBindingCount, g_productionChestGroupCount);
    }
}

static void ProductionOnMainThreadUpdate() {
    // Native status observers may synchronously re-enter the shared main
    // detour.  Only the outer callback owns the scan cursor/latches; a nested
    // callback must never consume a request or advance a scan in the middle of
    // an output transaction.
    if (g_productionMainCallbackDepth != 1) return;
    if (!ProductionFeatureIsEnabled()) {
        if (g_productionScanInProgress.load(std::memory_order_acquire) ||
            g_productionRegistryScanRequested.load(std::memory_order_acquire) ||
            g_productionWorldContextActive.load(std::memory_order_acquire)) {
            ProductionObserveWorldContext(false);
        }
        return;
    }
    if (!g_productionReady.load(std::memory_order_acquire) ||
        g_productionFaulted.load(std::memory_order_acquire) ||
        g_productionScanning) return;

    // Chain automation: periodically request a scan so positions are re-
    // evaluated even without a panel/hotkey trigger.  The exchange below
    // picks up the periodic request alongside any manual one.
    //
    // v1.1.26 optimization: instead of blindly firing every 20 s, use an
    // incremental trigger.  Peek at the live declared count (cheap: 1
    // pointer dereference + 1 u64 read) and only trigger a full scan when
    // the count changed since the last completed scan (player placed/removed
    // a gimmick) OR the 60 s backstop has elapsed.  This eliminates ~2/3 of
    // all-periodic scans during normal play where nothing changed.
    if (g_productionWorldContextActive.load(std::memory_order_acquire)) {
        const ULONGLONG now = GetTickCount64();
        ULONGLONG lastScan =
            g_productionAutoLinkLastScanTick.load(std::memory_order_acquire);
        const ULONGLONG elapsed = now - lastScan;
        const bool backstopReached =
            elapsed >= PRODUCTION_AUTOLINK_SCAN_BACKSTOP_MS;
        // Quick peek: read the save pointer and declared count without
        // entering a full scan.  If this fails (save not ready), fall back
        // to the old timer-based trigger.
        bool declaredChanged = false;
        if (backstopReached) {
            declaredChanged = true;  // force scan
        } else {
            // Peek at the declared count.  This is a best-effort check;
            // any failure means we fall back to the interval timer.
            const uintptr_t base =
                reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            void* root = nullptr;
            void* save = nullptr;
            if (ProductionReadPointer(
                    reinterpret_cast<void*>(base + RVA_GAME_ROOT), 0, &root) &&
                ProductionReadPointer(root, PRODUCTION_SAVE_OFFSET, &save) &&
                NearbyIsReadable(save, sizeof(void*)) &&
                *reinterpret_cast<const uintptr_t*>(save) ==
                    base + RVA_PRODUCTION_SAVE_DATA_VTABLE &&
                NearbyIsReadable(reinterpret_cast<unsigned char*>(save) +
                                     PRODUCTION_GIMMICK_LIST_OFFSET,
                                 sizeof(void*) + sizeof(u64))) {
                const u64 liveDeclared = *reinterpret_cast<const u64*>(
                    reinterpret_cast<unsigned char*>(save) +
                    PRODUCTION_GIMMICK_LIST_OFFSET + sizeof(void*));
                const size_t lastDeclared = g_productionLastScanDeclaredCount
                    .load(std::memory_order_acquire);
                declaredChanged =
                    (static_cast<size_t>(liveDeclared) != lastDeclared);
            } else {
                // Save not readable; fall back to interval timer.
                if (elapsed >= PRODUCTION_AUTOLINK_SCAN_INTERVAL_MS) {
                    declaredChanged = true;
                }
            }
        }
        if (declaredChanged) {
            // v1.1.29: Don't request a restart while a scan is already in
            // progress.  A yield keeps g_productionScanInProgress=true; the
            // scan resumes from the saved cursor on the next callback.
            // Requesting a restart here would reset the cursor to 0 (L7754)
            // and lose all batch progress, creating an infinite restart loop
            // that never reaches ProductionAutoLinkBindings (L8000) and thus
            // never creates any bindings (enabled_bindings=0).
            if (!g_productionScanInProgress.load(
                    std::memory_order_acquire) &&
                g_productionAutoLinkLastScanTick.compare_exchange_strong(
                    lastScan, now, std::memory_order_acq_rel)) {
                g_productionRegistryScanRequested.store(
                    true, std::memory_order_release);
            }
        }
    }

    const bool restart = g_productionRegistryScanRequested.exchange(
        false, std::memory_order_acq_rel);
    const bool globallyActive =
        g_productionScanInProgress.load(std::memory_order_acquire);
    if (!restart && !globallyActive) return;
    ProductionArmTransferCooldown(PRODUCTION_POST_SCAN_SETTLE_MS);
    const DWORD threadId = GetCurrentThreadId();
    const DWORD scanOwner =
        g_productionScanOwnerThread.load(std::memory_order_acquire);
    if (globallyActive && scanOwner != 0 && scanOwner != threadId) {
        g_productionFaulted.store(true, std::memory_order_release);
        ProductionEndScanAttempt(0, 0, true);
        ProductionObserveWorldContext(false);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
            "reason=main_thread_changed expected_thread=%lu actual_thread=%lu "
            "raw_pointer_cache=0 writes=0 latch_reset=1\n",
            scanOwner, threadId);
        return;
    }
    DWORD expected = 0;
    if (!g_productionMainThreadId.compare_exchange_strong(expected, threadId) &&
        expected != threadId) {
        g_productionFaulted.store(true, std::memory_order_release);
        ProductionEndScanAttempt(0, 0, true);
        ProductionObserveWorldContext(false);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=fault result=FAIL "
            "reason=main_thread_changed expected_thread=%lu actual_thread=%lu\n",
            expected, threadId);
        return;
    }
    if (!restart && !g_productionScanActive) {
        g_productionFaulted.store(true, std::memory_order_release);
        ProductionEndScanAttempt(0, 0, true);
        ProductionObserveWorldContext(false);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
            "reason=scan_owner_state_lost expected_thread=%lu "
            "actual_thread=%lu raw_pointer_cache=0 writes=0 latch_reset=1\n",
            threadId, threadId);
        return;
    }
    g_productionScanning = true;
    NearbyReadableRegionCache readableCache = {};
    const u64 scanLoadGeneration =
        g_autoPetLoadCompletionGeneration.load(std::memory_order_acquire);
    ProductionEnsurePersistentReadableCache(
        &readableCache, g_productionScanExpected, scanLoadGeneration);
    NearbyReadableRegionCacheScope readableCacheScope(&readableCache);
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    void* root = nullptr;
    void* save = nullptr;
    if (!ProductionReadPointer(reinterpret_cast<void*>(base + RVA_GAME_ROOT), 0,
                               &root) ||
        !ProductionReadPointer(root, PRODUCTION_SAVE_OFFSET, &save) ||
        !NearbyIsReadable(save, sizeof(void*)) ||
        *reinterpret_cast<const uintptr_t*>(save) !=
            base + RVA_PRODUCTION_SAVE_DATA_VTABLE ||
        !NearbyIsReadable(reinterpret_cast<unsigned char*>(save) + 0x3270,
                          sizeof(std::int64_t))) {
        ProductionEndScanAttempt(g_productionScanCursor,
                                 g_productionScanExpected, true);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
            "reason=save_unavailable processed=%zu total=%zu "
            "raw_pointer_cache=0 writes=0\n",
            g_productionScanCursor, g_productionScanExpected);
        return;
    }
    void* sentinel = nullptr;
    size_t declared = 0;
    if (!ProductionReadRegistryHead(save, &sentinel, &declared)) {
        ProductionEndScanAttempt(g_productionScanCursor,
                                 g_productionScanExpected, true);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
            "reason=registry_invalid processed=%zu total=%zu "
            "raw_pointer_cache=0 writes=0\n",
            g_productionScanCursor, g_productionScanExpected);
        return;
    }
    if (restart) {
        memset(g_productionScanObjects, 0, sizeof(g_productionScanObjects));
        g_productionScanCursor = 0;
        g_productionScanExpected = declared;
        g_productionScanObjectCount = 0;
        g_productionScanMachineCount = 0;
        g_productionScanNamedChestCount = 0;
        g_productionScanObjectTruncated = false;
        g_productionScanRegistryIndexCount = 0;
        g_productionScanChestStatusCount = 0;
        g_productionScanPrefixHash = 1469598103934665603ULL;
        g_productionScanResumeNode = nullptr;
        g_productionScanResumePrev = nullptr;
        g_productionScanStartedAt = NearbyPerformanceCounter();
        g_productionScanActive = true;
        g_productionScanOwnerThread.store(threadId, std::memory_order_release);
        g_productionScanInProgress.store(true, std::memory_order_release);
        ProductionPublishScanProgress(true, 0, declared, true);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_begin result=INCOMPLETE "
            "reason=hotkey_request total=%zu batch_limit=%zu "
            "all_map_registry=1 raw_pointer_cache=0 writes=0\n",
            declared, PRODUCTION_SCAN_BATCH_OBJECTS);
    } else if (declared != g_productionScanExpected) {
        ProductionEndScanAttempt(g_productionScanCursor, declared, true);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
            "reason=registry_generation_changed processed=%zu total_before=%zu "
            "total_after=%zu raw_pointer_cache=0 writes=0\n",
            g_productionScanCursor, g_productionScanExpected, declared);
        return;
    }

    void* node = nullptr;
    void* previous = nullptr;
    u64 observedPrefixHash = 1469598103934665603ULL;
    if (g_productionScanResumeNode && g_productionScanResumePrev &&
        NearbyIsReadable(g_productionScanResumeNode, sizeof(void*) * 3) &&
        *reinterpret_cast<void**>(
            reinterpret_cast<unsigned char*>(g_productionScanResumeNode) +
            sizeof(void*)) == g_productionScanResumePrev) {
        node = g_productionScanResumeNode;
        previous = g_productionScanResumePrev;
        observedPrefixHash = g_productionScanPrefixHash;
    } else {
        g_productionScanResumeNode = nullptr;
        g_productionScanResumePrev = nullptr;
        node = *reinterpret_cast<void**>(sentinel);
        previous = sentinel;
        for (size_t index = 0; index < g_productionScanCursor; ++index) {
        if (!node || node == sentinel ||
            !NearbyIsReadable(node, sizeof(void*) * 3) ||
            *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(node) +
                                      sizeof(void*)) != previous) {
            ProductionEndScanAttempt(g_productionScanCursor,
                                     g_productionScanExpected, true);
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
                "reason=registry_prefix_invalid processed=%zu total=%zu "
                "raw_pointer_cache=0 writes=0\n",
                g_productionScanCursor, g_productionScanExpected);
            return;
        }
        void* prefixStatus = *reinterpret_cast<void**>(
            reinterpret_cast<unsigned char*>(node) + sizeof(void*) * 2);
        ProductionStableId prefixIdentity = {};
        if (!ProductionReadGimmickIdentity(prefixStatus, &prefixIdentity)) {
            ProductionEndScanAttempt(g_productionScanCursor,
                                     g_productionScanExpected, true);
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
                "reason=registry_prefix_identity_invalid processed=%zu "
                "total=%zu stable_prefix_hash=0 raw_pointer_cache=0 "
                "writes=0\n",
                g_productionScanCursor, g_productionScanExpected);
            return;
        }
        observedPrefixHash = ProductionExtendStableIdentityHash(
            observedPrefixHash, prefixIdentity);
        previous = node;
        node = *reinterpret_cast<void**>(node);
    }
    }
    if (observedPrefixHash != g_productionScanPrefixHash) {
        const u64 expectedPrefixHash = g_productionScanPrefixHash;
        ProductionEndScanAttempt(g_productionScanCursor,
                                 g_productionScanExpected, true);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
            "reason=registry_prefix_identity_changed processed=%zu total=%zu "
            "prefix_hash_before=%llu prefix_hash_after=%llu "
            "raw_pointer_cache=0 writes=0\n",
            g_productionScanCursor, g_productionScanExpected,
            static_cast<unsigned long long>(expectedPrefixHash),
            static_cast<unsigned long long>(observedPrefixHash));
        return;
    }
    const size_t batchEnd = (g_productionScanExpected - g_productionScanCursor >
                             PRODUCTION_SCAN_BATCH_OBJECTS)
        ? g_productionScanCursor + PRODUCTION_SCAN_BATCH_OBJECTS
        : g_productionScanExpected;
    const LONGLONG scanBatchBegin = NearbyPerformanceCounter();
    while (g_productionScanCursor < batchEnd) {
        if (ProductionTicksToMicroseconds(
                NearbyPerformanceCounter() - scanBatchBegin) >
            PRODUCTION_SCAN_BUDGET_US) {
            break;
        }
        if (!node || node == sentinel ||
            !NearbyIsReadable(node, sizeof(void*) * 3) ||
            *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(node) +
                                      sizeof(void*)) != previous) {
            ProductionEndScanAttempt(g_productionScanCursor,
                                     g_productionScanExpected, true);
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
                "reason=registry_batch_invalid processed=%zu total=%zu "
                "raw_pointer_cache=0 writes=0\n",
                g_productionScanCursor, g_productionScanExpected);
            return;
        }
        void* status = *reinterpret_cast<void**>(
            reinterpret_cast<unsigned char*>(node) + sizeof(void*) * 2);
        ProductionStableId batchIdentity = {};
        if (!ProductionReadGimmickIdentity(status, &batchIdentity)) {
            ProductionEndScanAttempt(g_productionScanCursor,
                                     g_productionScanExpected, true);
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
                "reason=registry_batch_identity_invalid processed=%zu "
                "total=%zu stable_prefix_hash=0 raw_pointer_cache=0 "
                "writes=0\n",
                g_productionScanCursor, g_productionScanExpected);
            return;
        }
        observedPrefixHash = ProductionExtendStableIdentityHash(
            observedPrefixHash, batchIdentity);
        if (g_productionScanRegistryIndexCount <
                PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY &&
            g_productionScanCursor <= static_cast<size_t>(UINT32_MAX)) {
            ProductionRegistryIndexCacheEntry& entry =
                g_productionRegistryIndexBuild[
                    g_productionScanRegistryIndexCount++];
            entry.mapId = batchIdentity.mapId;
            entry.uniqueId = batchIdentity.uniqueId;
            entry.index = static_cast<u32>(g_productionScanCursor);
            entry.padding = 0;
        } else if (g_productionScanRegistryIndexCount ==
                   PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY) {
            ++g_productionScanRegistryIndexCount;
        }
        // --- Fast-path filter (v1.1.26 optimization) ---
        // Read only the gimmickId (1 ptr deref + 1 IsReadable) and use
        // ProductionIsMachineId to skip ~90% of registry objects that
        // are neither machines.  For non-machine IDs, still read the
        // module string inside ProductionReadObject to check chest/floor,
        // but skip the expensive identity re-read by passing preReadIdentity.
        u64 quickGimmickId = 0;
        const bool haveQuickId =
            ProductionReadGimmickIdOnly(status, &quickGimmickId);
        // If we have a valid ID and it's not a machine ID, we still need
        // to check if it's a chest or floor (which requires module string).
        // But if the ID IS a machine ID, we can skip the module read in
        // the fast path — ProductionReadObject will read it for the full
        // type check.  Either way, ProductionReadObject handles it.
        ProductionObjectSnapshot value = {};
        // v1.1.27→v1.1.28: readOutputMaps 仅对 machine 对象保留 true（被动机械
        // 如蜂箱/树液采集器依赖 output map 判定 OutputReady），chest/floor 跳过
        // 以减少 map 查找。skipRevalidation 回退为 false（v1.1.28b: true 导致链式绑定失效）。
        const bool isMachineId = haveQuickId && ProductionIsMachineId(quickGimmickId);
        if (ProductionReadObject(status, &value, true, isMachineId,
                                 &batchIdentity,
                                 haveQuickId ? quickGimmickId : 0,
                                 nullptr, false)) {
            if (value.machine) ++g_productionScanMachineCount;
            if (value.chest && value.named) ++g_productionScanNamedChestCount;
            if (g_productionScanObjectCount < PRODUCTION_MAX_OBJECT_SNAPSHOTS)
                g_productionScanObjects[g_productionScanObjectCount++] = value;
            else
                g_productionScanObjectTruncated = true;
            // Record the chest status pointer for input-fingerprint wakeups.
            if (value.chest &&
                g_productionScanChestStatusCount < PRODUCTION_MAX_CHESTS) {
                g_productionScanChestStatus[
                    g_productionScanChestStatusCount].id = value.id;
                g_productionScanChestStatus[
                    g_productionScanChestStatusCount].status = status;
                ++g_productionScanChestStatusCount;
            }
        }
        previous = node;
        node = *reinterpret_cast<void**>(node);
        ++g_productionScanCursor;
    }
    g_productionScanPrefixHash = observedPrefixHash;
    if (g_productionScanCursor < g_productionScanExpected) {
        g_productionScanResumeNode = node;
        g_productionScanResumePrev = previous;
        ProductionPublishScanProgress(true, g_productionScanCursor,
                                      g_productionScanExpected);
        g_productionScanning = false;
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_yield result=INCOMPLETE "
            "reason=batch_budget processed=%zu total=%zu batch_limit=%zu "
            "read_checks=%llu virtual_queries=%llu raw_pointer_cache=0 "
            "writes=0 stable_prefix_hash=%llu\n",
            g_productionScanCursor, g_productionScanExpected,
            PRODUCTION_SCAN_BATCH_OBJECTS,
            static_cast<unsigned long long>(readableCache.checks),
            static_cast<unsigned long long>(readableCache.virtualQueries),
            static_cast<unsigned long long>(g_productionScanPrefixHash));
        return;
    }
    if (node != sentinel ||
        *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(sentinel) +
                                  sizeof(void*)) != previous) {
        ProductionEndScanAttempt(g_productionScanCursor,
                                 g_productionScanExpected, true);
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=FAIL "
            "reason=registry_tail_invalid processed=%zu total=%zu "
            "raw_pointer_cache=0 writes=0\n",
            g_productionScanCursor, g_productionScanExpected);
        return;
    }
    // A completed on-demand scan has already walked every registry node and
    // validated every identity.  Publish the ordinal cache immediately so the
    // following morning-prime Dirty bindings resolve without one more full
    // registry walk on the main thread.
    ProductionRegistryIndexCachePublish(
        g_productionRegistryIndexBuild,
        g_productionScanRegistryIndexCount, g_productionScanExpected,
        g_autoPetLoadCompletionGeneration.load(std::memory_order_acquire));
    ++g_productionScanGeneration;
    ProductionObjectSnapshot* objectScratch = g_productionScanObjects;
    const size_t objectCount = g_productionScanObjectCount;
    const size_t machineCount = g_productionScanMachineCount;
    const size_t namedChestCount = g_productionScanNamedChestCount;
    const bool objectTruncated = g_productionScanObjectTruncated;
    const size_t statusCount = g_productionScanExpected;
    const std::int64_t gameSecond = *reinterpret_cast<const std::int64_t*>(
        reinterpret_cast<unsigned char*>(save) + 0x3270);

    AcquireSRWLockExclusive(&g_productionBindingLock);
    // Chain automation: auto-create/update bindings based on machine-chest
    // proximity before evaluating existing bindings.
    ProductionAutoLinkBindings(objectScratch, objectCount);

    // Evaluate all bindings with a time budget.  Each iteration may call
    // ProductionFindObject (O(N) linear scan), so 256 bindings × 768 objects
    // could be ~200K iterations.  Abort at 4 ms to protect the frame.
    const LONGLONG evalBegin = NearbyPerformanceCounter();
    static constexpr LONGLONG PRODUCTION_EVAL_BUDGET_US = 4000;
    bool evalBudgetExhausted = false;
    for (size_t index = 0; index < g_productionBindingCount; ++index) {
        if (ProductionTicksToMicroseconds(
                NearbyPerformanceCounter() - evalBegin) >
            PRODUCTION_EVAL_BUDGET_US) {
            evalBudgetExhausted = true;
            break;
        }
        ProductionBinding& binding = g_productionBindings[index];
        const ProductionState beforeState = binding.state;
        const ProductionReason beforeReason = binding.reason;
        ProductionEvaluateBinding(&binding, objectScratch, objectCount,
                                  gameSecond);
        // Input fingerprint change-trigger: if this binding is waiting for
        // ingredients and its input chest content hash changed since the last
        // failed attempt, wake it immediately so the new ingredients are
        // consumed on this scan instead of after the exponential backoff.
        if (binding.state == ProductionState::WaitingInput &&
            binding.hasInput && binding.inputFingerprint != 0) {
            const ProductionStatusHit* fpHit =
                ProductionFindScanChestStatus(binding.input);
            if (fpHit && fpHit->status) {
                const uint64_t fingerprint =
                    ProductionChestFingerprint(fpHit->status);
                if (fingerprint != binding.inputFingerprint) {
                    binding.inputFingerprint = 0;
                    ProductionDirtyBinding(&binding);
                    ProductionLog(
                        "[PRODAUTO] seq=%llu txn=0 event=input_fingerprint "
                        "result=PASS reason=chest_changed "
                        "device_map_id=%llu device_id=%llu input_map_id=%llu "
                        "input_id=%llu writes=0\n",
                        static_cast<unsigned long long>(
                            binding.machine.mapId),
                        static_cast<unsigned long long>(
                            binding.machine.uniqueId),
                        static_cast<unsigned long long>(
                            binding.input.mapId),
                        static_cast<unsigned long long>(
                            binding.input.uniqueId));
                }
            }
        }
        // A panel/full scan is never a schedule-prime entrance.  Preserve the
        // existing game-time deadline and backoff when the evaluated state is
        // unchanged; only a real state transition may publish a fresh schedule.
        // In particular a WaitingOutputSpace deadline must not be postponed by
        // opening the panel, and exponential backoff must not restart at zero.
        if (binding.state != beforeState || binding.reason != beforeReason) {
            const ProductionObjectSnapshot* scheduleMachine =
                ProductionFindObject(objectScratch, objectCount,
                                     binding.machine);
            ProductionScheduleBindingFromSnapshot(
                &binding, scheduleMachine, gameSecond);
        }
        if (binding.state != beforeState || binding.reason != beforeReason) {
            char inputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
            char outputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
            ProductionResolveLogName(objectScratch, objectCount, binding.input,
                                     binding.hasInput, inputName,
                                     sizeof(inputName));
            ProductionResolveLogName(objectScratch, objectCount, binding.output,
                                     binding.hasOutput, outputName,
                                     sizeof(outputName));
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=state device_map_id=%llu "
                "device_id=%llu input_map_id=%llu input_id=%llu "
                "output_map_id=%llu output_id=%llu input_name=%s "
                "output_name=%s state=%s from=%s to=%s result=INCOMPLETE "
                "reason=%s recipe=0 planned=0 removed=0 written=0 "
                "rollback=not_needed\n",
                static_cast<unsigned long long>(binding.machine.mapId),
                static_cast<unsigned long long>(binding.machine.uniqueId),
                static_cast<unsigned long long>(binding.input.mapId),
                static_cast<unsigned long long>(binding.input.uniqueId),
                static_cast<unsigned long long>(binding.output.mapId),
                static_cast<unsigned long long>(binding.output.uniqueId),
                inputName, outputName, ProductionStateName(binding.state),
                ProductionStateName(beforeState),
                ProductionStateName(binding.state),
                ProductionReasonName(binding.reason));
        }
    }
    if (evalBudgetExhausted) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=eval_budget result=INCOMPLETE "
                "reason=time_budget_exhausted total_bindings=%zu writes=0\n",
            g_productionBindingCount);
    }
    ReleaseSRWLockExclusive(&g_productionBindingLock);
    ProductionStorePersistentReadableCache(readableCache);
    ProductionPublish(objectScratch, objectCount, gameSecond);
    ProductionHudRefresh();

    // One requested scan emits one complete snapshot.  There is deliberately no
    // timer here: loading a save and leaving a panel open start no registry work.
    //
    // Performance: this loop does 3× O(N) lookups per binding plus a file write
    // per binding.  With 256 bindings and 768 objects that's ~600K iterations
    // plus 256 file writes every 10 seconds.  Gate on revision so it only runs
    // when something actually changed, and cap with a 2 ms time budget.
    static u64 s_lastSnapshotRevision = 0;
    const u64 snapshotRevision = g_productionBindingRevision.load(
        std::memory_order_acquire);
    const bool snapshotChanged = (snapshotRevision != s_lastSnapshotRevision);
    if (snapshotChanged) {
        s_lastSnapshotRevision = snapshotRevision;
        const LONGLONG snapBegin = NearbyPerformanceCounter();
        static constexpr LONGLONG PRODUCTION_SNAPSHOT_BUDGET_US = 2000;
        bool snapBudgetExhausted = false;
        AcquireSRWLockShared(&g_productionBindingLock);
        for (size_t index = 0; index < g_productionBindingCount; ++index) {
            if (ProductionTicksToMicroseconds(
                    NearbyPerformanceCounter() - snapBegin) >
                PRODUCTION_SNAPSHOT_BUDGET_US) {
                snapBudgetExhausted = true;
                break;
            }
            const ProductionBinding& binding = g_productionBindings[index];
            const ProductionObjectSnapshot* machine =
                ProductionFindObject(objectScratch, objectCount,
                                     binding.machine);
            const ProductionObjectSnapshot* input = binding.hasInput
                ? ProductionFindObject(objectScratch, objectCount,
                                       binding.input) : nullptr;
            const ProductionObjectSnapshot* output = binding.hasOutput
                ? ProductionFindObject(objectScratch, objectCount,
                                       binding.output) : nullptr;
            char inputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
            char outputName[PRODUCTION_MAX_NAME_BYTES + 1] = {};
            ProductionResolveLogName(objectScratch, objectCount, binding.input,
                                     binding.hasInput, inputName,
                                     sizeof(inputName));
            ProductionResolveLogName(objectScratch, objectCount, binding.output,
                                     binding.hasOutput, outputName,
                                     sizeof(outputName));
            u64 recipeId = 0;
            if (machine) {
                const size_t logSlotLimit = machine->slotLimit > 0 &&
                    machine->slotLimit <= PRODUCTION_MACHINE_SLOTS
                    ? machine->slotLimit : 0;
                for (size_t slot = 0; slot < logSlotLimit; ++slot) {
                    if (!machine->start[slot]) continue;
                    recipeId = ProductionInferRecipeId(machine, slot);
                    break;
                }
            }
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=binding_snapshot "
                "device_map_id=%llu device_id=%llu input_map_id=%llu "
                "input_id=%llu output_map_id=%llu output_id=%llu "
                "input_name=%s output_name=%s state=%s from=%s to=%s "
                "recipe=%llu planned=0 removed=0 written=0 "
                "rollback=not_needed result=INCOMPLETE reason=%s "
                "game_second=%lld machine_resolved=%d input_resolved=%d "
                "output_resolved=%d slot_limit=%zu start0=%llu duration0=%llu "
                "output_item0=%llu output_count0=%d start1=%llu "
                "duration1=%llu output_item1=%llu output_count1=%d "
                "start2=%llu duration2=%llu output_item2=%llu "
                "output_count2=%d\n",
                static_cast<unsigned long long>(binding.machine.mapId),
                static_cast<unsigned long long>(binding.machine.uniqueId),
                static_cast<unsigned long long>(binding.input.mapId),
                static_cast<unsigned long long>(binding.input.uniqueId),
                static_cast<unsigned long long>(binding.output.mapId),
                static_cast<unsigned long long>(binding.output.uniqueId),
                inputName, outputName, ProductionStateName(binding.state),
                ProductionStateName(binding.state),
                ProductionStateName(binding.state),
                static_cast<unsigned long long>(recipeId),
                ProductionReasonName(binding.reason),
                static_cast<long long>(gameSecond), machine ? 1 : 0,
                input ? 1 : 0, output ? 1 : 0,
                machine ? machine->slotLimit : 0,
                static_cast<unsigned long long>(machine ? machine->start[0] : 0),
                static_cast<unsigned long long>(machine ? machine->duration[0] : 0),
                static_cast<unsigned long long>(machine ? machine->outputItemId[0] : 0),
                machine ? machine->outputCount[0] : 0,
                static_cast<unsigned long long>(machine ? machine->start[1] : 0),
                static_cast<unsigned long long>(machine ? machine->duration[1] : 0),
                static_cast<unsigned long long>(machine ? machine->outputItemId[1] : 0),
                machine ? machine->outputCount[1] : 0,
                static_cast<unsigned long long>(machine ? machine->start[2] : 0),
                static_cast<unsigned long long>(machine ? machine->duration[2] : 0),
                static_cast<unsigned long long>(machine ? machine->outputItemId[2] : 0),
                machine ? machine->outputCount[2] : 0);
        }
        ReleaseSRWLockShared(&g_productionBindingLock);
        if (snapBudgetExhausted) {
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=snapshot_budget "
                "result=INCOMPLETE reason=time_budget_exhausted "
                "total_bindings=%zu writes=0\n",
                g_productionBindingCount);
        }
    }
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=all_map_scan result=PASS "
        "reason=none game_second=%lld "
        "registry=%zu objects=%zu machines=%zu named_chests=%zu "
        "truncated=%d cross_map_registry_observed=1 on_demand=1 "
        "raw_pointer_cache=0 writes=0\n",
        static_cast<long long>(gameSecond), statusCount, objectCount,
        machineCount, namedChestCount, objectTruncated ? 1 : 0);

    const double elapsedMs = NearbyElapsedMilliseconds(
        g_productionScanStartedAt, NearbyPerformanceCounter());
    const size_t completed = g_productionScanCursor;
    const size_t expectedCount = g_productionScanExpected;
    ProductionEndScanAttempt(completed, expectedCount, false);
    g_productionScanCursor = 0;
    g_productionScanExpected = 0;
    g_productionScanResumeNode = nullptr;
    g_productionScanResumePrev = nullptr;
    // v1.1.26: record the declared count from this completed scan so the
    // incremental trigger can detect changes on the next periodic check.
    g_productionLastScanDeclaredCount.store(expectedCount,
                                            std::memory_order_release);
    // Log only after mod-owned state and the recursion latch are reset.  This
    // closes the exact evidence gap in the reported frozen build.
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=scan_finish result=PASS "
        "reason=none processed=%zu total=%zu "
        "objects=%zu machines=%zu named_chests=%zu elapsed_ms=%.3f "
        "read_checks=%llu virtual_queries=%llu batch_limit=%zu "
        "raw_pointer_cache=0 writes=0 latch_reset=1 "
        "stable_prefix_hash=%llu\n",
        completed, expectedCount, objectCount, machineCount, namedChestCount,
        elapsedMs, static_cast<unsigned long long>(readableCache.checks),
        static_cast<unsigned long long>(readableCache.virtualQueries),
        PRODUCTION_SCAN_BATCH_OBJECTS,
        static_cast<unsigned long long>(g_productionScanPrefixHash));
}

struct ProductionSelectedMachine {
    void* status; // synchronous main-callback borrow; never published or cached
    void* controller; // same synchronous borrow; re-read before input consume
    ProductionStableId id;
    u64 objectId; // engine-selected collision/sensor object
    u64 machineObjectId; // root object obtained from the chosen status
    u64 commandId;
    u64 gimmickId;
    float targetPosition[3];
    float targetAngle;
    float machineDistance;
    size_t candidateCount;
    size_t parentDepth;
    bool parentChainRoute;
    char module[PRODUCTION_MAX_MODULE_BYTES + 1];
};

static bool ProductionReadStatusObjectId(void* status, u64* objectId) {
    void* linkA = nullptr;
    void* linkB = nullptr;
    if (!objectId ||
        !ProductionReadPointer(status, PRODUCTION_STATUS_OBJECT_LINK_OFFSET,
                               &linkA) ||
        !ProductionReadPointer(linkA, PRODUCTION_LINK_BODY_OFFSET, &linkB) ||
        !NearbyIsReadable(reinterpret_cast<unsigned char*>(linkB) +
                              PRODUCTION_WORLD_OBJECT_ID_OFFSET,
                          sizeof(u64))) return false;
    *objectId = *reinterpret_cast<const u64*>(
        reinterpret_cast<unsigned char*>(linkB) +
        PRODUCTION_WORLD_OBJECT_ID_OFFSET);
    return *objectId != 0;
}

static bool ProductionReadSelectedInteraction(
        void* controller, u64* objectId, u64* commandId,
        float* targetPosition, float* targetAngle) {
    if (!controller || !objectId || !commandId || !targetPosition ||
        !targetAngle) return false;
    const unsigned char* selected =
        reinterpret_cast<const unsigned char*>(controller) +
        PRODUCTION_SELECTED_OBJECT_ID_OFFSET;
    if (!NearbyIsReadable(selected,
                          PRODUCTION_SELECTED_COMMAND_HOLDER_OFFSET -
                              PRODUCTION_SELECTED_OBJECT_ID_OFFSET +
                              sizeof(void*))) return false;
    void* commandHolder = nullptr;
    void* commandData = nullptr;
    if (!ProductionReadPointer(controller,
                               PRODUCTION_SELECTED_COMMAND_HOLDER_OFFSET,
                               &commandHolder) ||
        !ProductionReadPointer(commandHolder, 0, &commandData) ||
        !NearbyIsReadable(commandData, sizeof(u64))) return false;
    *objectId = *reinterpret_cast<const u64*>(selected);
    *commandId = *reinterpret_cast<const u64*>(commandData);
    memcpy(targetPosition,
           reinterpret_cast<const unsigned char*>(controller) +
               PRODUCTION_SELECTED_TARGET_POSITION_OFFSET,
           sizeof(float) * 3);
    *targetAngle = *reinterpret_cast<const float*>(
        reinterpret_cast<const unsigned char*>(controller) +
        PRODUCTION_SELECTED_TARGET_ANGLE_OFFSET);
    return *objectId != 0 && std::isfinite(targetPosition[0]) &&
           std::isfinite(targetPosition[1]) &&
           std::isfinite(targetPosition[2]) && std::isfinite(*targetAngle);
}

static bool ProductionValidateLoadedMachineStatus(
        void* status, ProductionStableId* identity, u64* gimmickId,
        char* module, size_t moduleSize, u64* rootObjectId) {
    if (!status || !identity || !gimmickId || !module || moduleSize < 2 ||
        !rootObjectId || !g_nearbyResolveWorldObject ||
        !g_productionResolveComponent) return false;
    if (!ProductionReadGimmickIdentity(status, identity) ||
        !ProductionReadGimmickType(status, gimmickId, module, moduleSize) ||
        !ProductionIsMachineType(*gimmickId, module) ||
        !ProductionReadStatusObjectId(status, rootObjectId)) return false;
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    void** worldRegistry = reinterpret_cast<void**>(
        base + RVA_NEARBY_WORLD_OBJECT_REGISTRY);
    if (!NearbyIsReadable(worldRegistry, sizeof(void*)) || !*worldRegistry)
        return false;
    void* worldObject =
        g_nearbyResolveWorldObject(*worldRegistry, *rootObjectId);
    if (!worldObject) return false;
    void* component = g_productionResolveComponent(worldObject);
    if (!component ||
        !NearbyIsReadable(component,
                          PRODUCTION_COMPONENT_STATUS_OFFSET + sizeof(void*)) ||
        *reinterpret_cast<const uintptr_t*>(component) !=
            base + RVA_PRODUCTION_COMPONENT_VTABLE) return false;
    void* roundTripStatus = *reinterpret_cast<void**>(
        reinterpret_cast<unsigned char*>(component) +
        PRODUCTION_COMPONENT_STATUS_OFFSET);
    return roundTripStatus == status;
}

static bool ProductionParentOwnsChild(void* parent, void* child) {
    if (!parent || !child || parent == child ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(parent) +
                PRODUCTION_WORLD_CHILDREN_BEGIN_OFFSET,
            PRODUCTION_WORLD_CHILDREN_CAPACITY_OFFSET -
                    PRODUCTION_WORLD_CHILDREN_BEGIN_OFFSET +
                sizeof(void*))) return false;
    void** begin = *reinterpret_cast<void***>(
        reinterpret_cast<unsigned char*>(parent) +
        PRODUCTION_WORLD_CHILDREN_BEGIN_OFFSET);
    void** end = *reinterpret_cast<void***>(
        reinterpret_cast<unsigned char*>(parent) +
        PRODUCTION_WORLD_CHILDREN_END_OFFSET);
    void** capacity = *reinterpret_cast<void***>(
        reinterpret_cast<unsigned char*>(parent) +
        PRODUCTION_WORLD_CHILDREN_CAPACITY_OFFSET);
    const uintptr_t beginValue = reinterpret_cast<uintptr_t>(begin);
    const uintptr_t endValue = reinterpret_cast<uintptr_t>(end);
    const uintptr_t capacityValue = reinterpret_cast<uintptr_t>(capacity);
    if (!beginValue || endValue < beginValue || capacityValue < endValue ||
        (endValue - beginValue) % sizeof(void*) != 0 ||
        (capacityValue - beginValue) % sizeof(void*) != 0) return false;
    const size_t count = (endValue - beginValue) / sizeof(void*);
    const size_t capacityCount =
        (capacityValue - beginValue) / sizeof(void*);
    if (!count || count > PRODUCTION_MAX_PARENT_CHILDREN ||
        capacityCount < count ||
        capacityCount > PRODUCTION_MAX_PARENT_CHILDREN ||
        !NearbyIsReadable(begin, count * sizeof(void*))) return false;
    size_t matches = 0;
    for (size_t index = 0; index < count; ++index) {
        if (begin[index] == child) ++matches;
    }
    return matches == 1;
}

static bool ProductionWorldObjectRoundTrips(void* registry,
                                            void* worldObject,
                                            u64* objectId) {
    if (!registry || !worldObject || !objectId ||
        !g_nearbyResolveWorldObject ||
        !NearbyIsReadable(
            reinterpret_cast<unsigned char*>(worldObject) +
                PRODUCTION_WORLD_OBJECT_ID_OFFSET,
            sizeof(u64))) return false;
    *objectId = *reinterpret_cast<const u64*>(
        reinterpret_cast<unsigned char*>(worldObject) +
        PRODUCTION_WORLD_OBJECT_ID_OFFSET);
    return *objectId != 0 &&
           g_nearbyResolveWorldObject(registry, *objectId) == worldObject;
}

static bool ProductionResolveMachineFromSelectedHierarchy(
        void* selectedWorldObject, const float* targetPosition,
        ProductionSelectedMachine* output, const char** failureReason) {
    if (failureReason) *failureReason = "selected_owner_chain_unavailable";
    if (!selectedWorldObject || !targetPosition || !output ||
        !g_productionResolveComponent || !g_nearbyResolveWorldObject)
        return false;
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    void** worldRegistry = reinterpret_cast<void**>(
        base + RVA_NEARBY_WORLD_OBJECT_REGISTRY);
    if (!NearbyIsReadable(worldRegistry, sizeof(void*)) || !*worldRegistry)
        return false;

    void* visited[PRODUCTION_MAX_PARENT_DEPTH + 1] = {};
    void* current = selectedWorldObject;
    for (size_t depth = 0; depth <= PRODUCTION_MAX_PARENT_DEPTH; ++depth) {
        if (!current ||
            !NearbyIsReadable(
                current,
                PRODUCTION_WORLD_CHILDREN_CAPACITY_OFFSET + sizeof(void*))) {
            if (failureReason) *failureReason = "selected_owner_chain_invalid";
            return false;
        }
        for (size_t prior = 0; prior < depth; ++prior) {
            if (visited[prior] == current) {
                if (failureReason) *failureReason = "selected_owner_chain_cycle";
                return false;
            }
        }
        visited[depth] = current;
        output->parentDepth = depth;
        output->parentChainRoute = depth != 0;
        u64 hierarchyObjectId = 0;
        if (!ProductionWorldObjectRoundTrips(*worldRegistry, current,
                                             &hierarchyObjectId) ||
            (depth == 0 && hierarchyObjectId != output->objectId)) {
            if (failureReason)
                *failureReason = "selected_owner_registry_roundtrip_failed";
            return false;
        }

        void* component = g_productionResolveComponent(current);
        if (component &&
            NearbyIsReadable(component,
                             PRODUCTION_COMPONENT_STATUS_OFFSET +
                                 sizeof(void*)) &&
            *reinterpret_cast<const uintptr_t*>(component) ==
                base + RVA_PRODUCTION_COMPONENT_VTABLE) {
            void* status = *reinterpret_cast<void**>(
                reinterpret_cast<unsigned char*>(component) +
                PRODUCTION_COMPONENT_STATUS_OFFSET);
            ProductionStableId identity = {};
            u64 gimmickId = 0;
            u64 rootObjectId = 0;
            char module[PRODUCTION_MAX_MODULE_BYTES + 1] = {};
            if (ProductionValidateLoadedMachineStatus(
                    status, &identity, &gimmickId, module, sizeof(module),
                    &rootObjectId) &&
                g_nearbyResolveWorldObject(*worldRegistry, rootObjectId) ==
                    current) {
                output->status = status;
                output->id = identity;
                output->machineObjectId = rootObjectId;
                output->gimmickId = gimmickId;
                output->candidateCount = 1;
                if (NearbyIsReadable(
                        reinterpret_cast<unsigned char*>(status) +
                            PRODUCTION_POSITION_OFFSET,
                        sizeof(float) * 3)) {
                    const float* position = reinterpret_cast<const float*>(
                        reinterpret_cast<unsigned char*>(status) +
                        PRODUCTION_POSITION_OFFSET);
                    const float deltaX = position[0] - targetPosition[0];
                    const float deltaY = position[1] - targetPosition[1];
                    const float distanceSquared =
                        deltaX * deltaX + deltaY * deltaY;
                    if (std::isfinite(distanceSquared))
                        output->machineDistance = sqrtf(distanceSquared);
                }
                memcpy(output->module, module, sizeof(output->module));
                if (failureReason) *failureReason = "none";
                return true;
            }
        }

        if (depth == PRODUCTION_MAX_PARENT_DEPTH) {
            if (failureReason) *failureReason = "selected_owner_depth_exceeded";
            return false;
        }
        void* parent = nullptr;
        if (!ProductionReadPointer(current, PRODUCTION_WORLD_PARENT_OFFSET,
                                   &parent) || !parent) {
            if (failureReason) *failureReason = "selected_owner_machine_missing";
            return false;
        }
        output->parentChainRoute = true;
        if (!ProductionParentOwnsChild(parent, current)) {
            if (failureReason) *failureReason = "selected_owner_reverse_link_invalid";
            return false;
        }
        current = parent;
    }
    if (failureReason) *failureReason = "selected_owner_machine_missing";
    return false;
}


static bool ProductionResolveSelectedMachine(ProductionSelectedMachine* output,
                                             const char** failureReason) {
    if (output) *output = {};
    if (failureReason) *failureReason = "target_chain_unavailable";
    if (!output || !g_nearbyResolveWorldObject ||
        !g_productionResolveComponent) return false;
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    void* root = nullptr;
    void* player = nullptr;
    void* objectStatus = nullptr;
    void* controller = nullptr;
    if (!ProductionReadPointer(reinterpret_cast<void*>(base + RVA_GAME_ROOT), 0,
                               &root) ||
        !ProductionReadPointer(root, PRODUCTION_SAVE_OFFSET, &player) ||
        !ProductionReadPointer(player, PRODUCTION_PLAYER_STATUS_OFFSET,
                               &objectStatus) ||
        !ProductionReadPointer(objectStatus,
                               PRODUCTION_PLAYER_CONTROLLER_OFFSET,
                               &controller)) return false;

    u64 selectedObjectId = 0;
    u64 selectedCommandId = 0;
    float targetPosition[3] = {};
    float targetAngle = 0.0f;
    if (!ProductionReadSelectedInteraction(
            controller, &selectedObjectId, &selectedCommandId,
            targetPosition, &targetAngle)) return false;
    output->controller = controller;
    output->objectId = selectedObjectId;
    output->commandId = selectedCommandId;
    memcpy(output->targetPosition, targetPosition,
           sizeof(output->targetPosition));
    output->targetAngle = targetAngle;
    if (!selectedObjectId ||
        (selectedCommandId != PRODUCTION_ITEM_IN_COMMAND &&
         selectedCommandId != PRODUCTION_ITEM_OUT_COMMAND)) {
        if (failureReason) *failureReason = "native_target_not_item_in_out";
        return false;
    }

    void** worldRegistry = reinterpret_cast<void**>(
        base + RVA_NEARBY_WORLD_OBJECT_REGISTRY);
    if (!NearbyIsReadable(worldRegistry, sizeof(void*)) || !*worldRegistry)
        return false;
    void* worldObject =
        g_nearbyResolveWorldObject(*worldRegistry, selectedObjectId);
    if (!worldObject) {
        if (failureReason) *failureReason = "selected_world_object_missing";
        return false;
    }
    // The native selected record already identifies one exact interaction
    // sensor.  Resolve its verified owner hierarchy; never reselect a machine
    // by counting or ranking nearby status objects.
    return ProductionResolveMachineFromSelectedHierarchy(
        worldObject, targetPosition, output, failureReason);
}static bool ProductionVerifyNativeGraph() {
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    static const unsigned char targetSelectBytes[] = SIG_PRODUCTION_TARGET_SELECT;
    static const unsigned char targetAcceptBytes[] = SIG_PRODUCTION_TARGET_ACCEPT;
    static const unsigned char componentResolverBytes[] = SIG_PRODUCTION_RESOLVE_COMPONENT;
    static const unsigned char objectGetParentBytes[] = SIG_PRODUCTION_OBJECT_GET_PARENT;
    static const unsigned char logicalChildAttachBytes[] = SIG_PRODUCTION_LOGICAL_CHILD_ATTACH;
    static const unsigned char logicalParentReadBytes[] = SIG_PRODUCTION_LOGICAL_PARENT_READ;
    static const unsigned char logicalChildVectorBytes[] = SIG_PRODUCTION_LOGICAL_CHILD_VECTOR;
    static const unsigned char pushBytes[] = SIG_PRODUCTION_PUSH;
    static const unsigned char popBytes[] = SIG_PRODUCTION_POP;
    static const unsigned char outputBytes[] = SIG_PRODUCTION_OUTPUT_HELPER;
    static const unsigned char itemAllocateBytes[] = SIG_PRODUCTION_ITEM_ALLOCATE;
    static const unsigned char itemCtorBytes[] = SIG_PRODUCTION_ITEM_CTOR;
    static const unsigned char itemSetCountBytes[] = SIG_PRODUCTION_ITEM_SET_COUNT;
    static const unsigned char itemIsStackableBytes[] = SIG_PRODUCTION_ITEM_IS_STACKABLE;
    static const unsigned char chestPushBytes[] = SIG_PRODUCTION_CHEST_PUSH;
    static const unsigned char chestCommitBytes[] = SIG_PRODUCTION_CHEST_COMMIT;
    static const unsigned char statusIntLookupBytes[] = SIG_PRODUCTION_STATUS_INT_LOOKUP;
    static const unsigned char statusU64LookupBytes[] = SIG_PRODUCTION_STATUS_U64_LOOKUP;
    static const unsigned char statusEventLookupBytes[] = SIG_PRODUCTION_STATUS_EVENT_LOOKUP;
    static const unsigned char statusEventNotifyBytes[] = SIG_PRODUCTION_STATUS_EVENT_NOTIFY;
    static const unsigned char nativeLoadBytes[] = SIG_PRODUCTION_NATIVE_LOAD;
    static const unsigned char nativeCollectBytes[] = SIG_PRODUCTION_NATIVE_COLLECT;
    struct Expected {
        const char* name;
        uintptr_t rva;
        const unsigned char* bytes;
        size_t size;
    };
    const Expected expected[] = {
        {"target_select", RVA_PRODUCTION_TARGET_SELECT, targetSelectBytes,
         sizeof(targetSelectBytes)},
        {"target_accept", RVA_PRODUCTION_TARGET_ACCEPT, targetAcceptBytes,
         sizeof(targetAcceptBytes)},
        {"component_resolver", RVA_PRODUCTION_RESOLVE_COMPONENT,
         componentResolverBytes, sizeof(componentResolverBytes)},
        {"object_get_parent", RVA_PRODUCTION_OBJECT_GET_PARENT,
         objectGetParentBytes, sizeof(objectGetParentBytes)},
        {"logical_child_attach", RVA_PRODUCTION_LOGICAL_CHILD_ATTACH,
         logicalChildAttachBytes, sizeof(logicalChildAttachBytes)},
        {"logical_parent_read", RVA_PRODUCTION_LOGICAL_PARENT_READ,
         logicalParentReadBytes, sizeof(logicalParentReadBytes)},
        {"logical_child_vector", RVA_PRODUCTION_LOGICAL_CHILD_VECTOR,
         logicalChildVectorBytes, sizeof(logicalChildVectorBytes)},
        {"push", RVA_PRODUCTION_PUSH, pushBytes, sizeof(pushBytes)},
        {"pop", RVA_PRODUCTION_POP, popBytes, sizeof(popBytes)},
        {"output_helper", RVA_PRODUCTION_OUTPUT_HELPER, outputBytes, sizeof(outputBytes)},
        {"item_allocate", RVA_PRODUCTION_ITEM_ALLOCATE,
         itemAllocateBytes, sizeof(itemAllocateBytes)},
        {"item_ctor", RVA_PRODUCTION_ITEM_CTOR, itemCtorBytes, sizeof(itemCtorBytes)},
        {"item_set_count", RVA_PRODUCTION_ITEM_SET_COUNT,
         itemSetCountBytes, sizeof(itemSetCountBytes)},
        {"item_is_stackable", RVA_PRODUCTION_ITEM_IS_STACKABLE,
         itemIsStackableBytes, sizeof(itemIsStackableBytes)},
        {"chest_push", RVA_PRODUCTION_CHEST_PUSH, chestPushBytes, sizeof(chestPushBytes)},
        {"chest_commit", RVA_PRODUCTION_CHEST_COMMIT, chestCommitBytes, sizeof(chestCommitBytes)},
        {"status_int_lookup", RVA_PRODUCTION_STATUS_INT_LOOKUP,
         statusIntLookupBytes, sizeof(statusIntLookupBytes)},
        {"status_u64_lookup", RVA_PRODUCTION_STATUS_U64_LOOKUP,
         statusU64LookupBytes, sizeof(statusU64LookupBytes)},
        {"status_event_lookup", RVA_PRODUCTION_STATUS_EVENT_LOOKUP,
         statusEventLookupBytes, sizeof(statusEventLookupBytes)},
        {"status_event_notify", RVA_PRODUCTION_STATUS_EVENT_NOTIFY,
         statusEventNotifyBytes, sizeof(statusEventNotifyBytes)},
        {"native_load", RVA_PRODUCTION_NATIVE_LOAD,
         nativeLoadBytes, sizeof(nativeLoadBytes)},
        {"native_collect", RVA_PRODUCTION_NATIVE_COLLECT,
         nativeCollectBytes, sizeof(nativeCollectBytes)},
    };
    for (const Expected& item : expected) {
        if (!NearbyIsReadable(reinterpret_cast<void*>(base + item.rva),
                              item.size) ||
            memcmp(reinterpret_cast<void*>(base + item.rva), item.bytes,
                   item.size) != 0) {
            ProductionLog(
                "[PRODAUTO] seq=%llu txn=0 event=install result=FAIL "
                "reason=native_signature name=%s rva=0x%llx\n", item.name,
                static_cast<unsigned long long>(item.rva));
            return false;
        }
    }
    if (*reinterpret_cast<const uintptr_t*>(base + RVA_PRODUCTION_GIMMICK_VTABLE) !=
            base + RTTI_GIMMICK_VTABLE_TARGET ||
        *reinterpret_cast<const uintptr_t*>(
             base + RVA_PRODUCTION_GIMMICK_SECONDARY_VTABLE) !=
            base + RTTI_GIMMICK_SECONDARY_VTABLE_TARGET ||
        *reinterpret_cast<const uintptr_t*>(base + RVA_PRODUCTION_MAP_STATUS_VTABLE) !=
            base + RTTI_MAP_STATUS_VTABLE_TARGET ||
        *reinterpret_cast<const uintptr_t*>(base + RVA_PRODUCTION_SAVE_DATA_VTABLE) !=
            base + RTTI_SAVE_DATA_VTABLE_TARGET ||
        *reinterpret_cast<const uintptr_t*>(base + RVA_PRODUCTION_COMPONENT_VTABLE) !=
            base + RTTI_COMPONENT_VTABLE_TARGET) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=install result=FAIL "
            "reason=rtti_vtable_graph\n");
        return false;
    }
    struct CompleteObjectLocator {
        u32 signature;
        u32 offset;
        u32 cdOffset;
        u32 typeDescriptorRva;
        u32 hierarchyRva;
        u32 selfRva;
    };
    const CompleteObjectLocator* gimmickLocator =
        reinterpret_cast<const CompleteObjectLocator*>(base + RTTI_GIMMICK_COL);
    const CompleteObjectLocator* gimmickSecondaryLocator =
        reinterpret_cast<const CompleteObjectLocator*>(base + RTTI_GIMMICK_SECONDARY_COL);
    const CompleteObjectLocator* saveLocator =
        reinterpret_cast<const CompleteObjectLocator*>(base + RTTI_SAVE_DATA_COL);
    const CompleteObjectLocator* mapLocator =
        reinterpret_cast<const CompleteObjectLocator*>(base + RTTI_MAP_STATUS_COL);
    if (!NearbyIsReadable(gimmickLocator, sizeof(*gimmickLocator)) ||
        !NearbyIsReadable(gimmickSecondaryLocator,
                          sizeof(*gimmickSecondaryLocator)) ||
        !NearbyIsReadable(saveLocator, sizeof(*saveLocator)) ||
        !NearbyIsReadable(mapLocator, sizeof(*mapLocator)) ||
        *reinterpret_cast<const uintptr_t*>(
             base + RVA_PRODUCTION_GIMMICK_VTABLE - sizeof(void*)) !=
             base + RTTI_GIMMICK_COL ||
        *reinterpret_cast<const uintptr_t*>(
             base + RVA_PRODUCTION_GIMMICK_SECONDARY_VTABLE - sizeof(void*)) !=
             base + RTTI_GIMMICK_SECONDARY_COL ||
        *reinterpret_cast<const uintptr_t*>(
             base + RVA_PRODUCTION_SAVE_DATA_VTABLE - sizeof(void*)) !=
             base + RTTI_SAVE_DATA_COL ||
        *reinterpret_cast<const uintptr_t*>(
             base + RVA_PRODUCTION_MAP_STATUS_VTABLE - sizeof(void*)) !=
             base + RTTI_MAP_STATUS_COL ||
        gimmickLocator->signature != 1 || gimmickLocator->offset != 0 ||
        gimmickLocator->typeDescriptorRva != RTTI_GIMMICK_TYPE_DESCRIPTOR ||
        gimmickLocator->hierarchyRva != RTTI_GIMMICK_SECONDARY_HIERARCHY ||
        gimmickLocator->selfRva != RTTI_GIMMICK_COL ||
        gimmickSecondaryLocator->signature != 1 ||
        gimmickSecondaryLocator->offset != RTTI_GIMMICK_SECONDARY_OFFSET ||
        gimmickSecondaryLocator->cdOffset != 0 ||
        gimmickSecondaryLocator->typeDescriptorRva != RTTI_GIMMICK_TYPE_DESCRIPTOR ||
        gimmickSecondaryLocator->hierarchyRva != RTTI_GIMMICK_SECONDARY_HIERARCHY ||
        gimmickSecondaryLocator->selfRva != RTTI_GIMMICK_SECONDARY_COL ||
        saveLocator->signature != 1 ||
        saveLocator->typeDescriptorRva != RTTI_SAVE_DATA_TYPE_DESCRIPTOR ||
        saveLocator->selfRva != RTTI_SAVE_DATA_COL ||
        mapLocator->signature != 1 ||
        mapLocator->typeDescriptorRva != RTTI_MAP_STATUS_TYPE_DESCRIPTOR ||
        mapLocator->selfRva != RTTI_MAP_STATUS_COL ||
        !NearbyIsReadable(reinterpret_cast<void*>(
             base + RTTI_GIMMICK_TYPE_DESCRIPTOR + 16), 22) ||
        !NearbyIsReadable(reinterpret_cast<void*>(
             base + RTTI_SAVE_DATA_TYPE_DESCRIPTOR + 16), 17) ||
        !NearbyIsReadable(reinterpret_cast<void*>(
             base + RTTI_MAP_STATUS_TYPE_DESCRIPTOR + 16), 24) ||
        strcmp(reinterpret_cast<const char*>(
             base + RTTI_GIMMICK_TYPE_DESCRIPTOR + 16),
               RTTI_GIMMICK_TYPE_NAME) != 0 ||
        strcmp(reinterpret_cast<const char*>(
             base + RTTI_SAVE_DATA_TYPE_DESCRIPTOR + 16),
               RTTI_SAVE_DATA_TYPE_NAME) != 0 ||
        strcmp(reinterpret_cast<const char*>(
             base + RTTI_MAP_STATUS_TYPE_DESCRIPTOR + 16),
               RTTI_MAP_STATUS_TYPE_NAME) != 0) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=install result=FAIL "
            "reason=rtti_col_type_descriptor\n");
        return false;
    }
    return true;
}

static bool InstallProductionAutomation() {
    if (!PackageWritesAuthorized() || !ProductionBuildSealValid() ||
        !g_nearbySortReady.load(std::memory_order_acquire) ||
        !g_nearbyResolveWorldObject || !g_nearbyInputActive ||
        !g_nearbyInputEvent || !g_nearbyInputConsume ||
        !g_productionMainWorldGameClockSourceReady.load(
            std::memory_order_acquire) ||
        !ProductionVerifyNativeGraph()) {
        ProductionLog(
            "[PRODAUTO] seq=%llu txn=0 event=install result=FAIL "
            "reason=preflight\n");
        return false;
    }
    // No processing-UI detour is installed.  Those native calls create the
    // world-space ProcessingMachineRoot prompt, not an equipment menu.  Target
    // selection is read from the already-installed CState_Main dispatcher.
    const uintptr_t base =
        reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    g_productionResolveComponent =
        reinterpret_cast<ProductionResolveComponentFunction>(
            base + RVA_PRODUCTION_RESOLVE_COMPONENT);
    g_productionOutputHelper =
        reinterpret_cast<ProductionOutputHelperFunction>(
            base + RVA_PRODUCTION_OUTPUT_HELPER);
    g_productionItemAllocate =
        reinterpret_cast<ProductionItemAllocateFunction>(
            base + RVA_PRODUCTION_ITEM_ALLOCATE);
    g_productionItemCtor =
        reinterpret_cast<ProductionItemCtorFunction>(
            base + RVA_PRODUCTION_ITEM_CTOR);
    g_productionItemSetCount =
        reinterpret_cast<ProductionItemSetCountFunction>(
            base + RVA_PRODUCTION_ITEM_SET_COUNT);
    g_productionItemIsStackable =
        reinterpret_cast<ProductionItemIsStackableFunction>(
            base + RVA_PRODUCTION_ITEM_IS_STACKABLE);
    g_productionNativeLoad =
        reinterpret_cast<ProductionNativeLoadFunction>(
            base + RVA_PRODUCTION_NATIVE_LOAD);
    g_productionNativeCollect =
        reinterpret_cast<ProductionNativeCollectFunction>(
            base + RVA_PRODUCTION_NATIVE_COLLECT);
    g_productionStatusIntLookup =
        reinterpret_cast<ProductionStatusMapLookupFunction>(
            base + RVA_PRODUCTION_STATUS_INT_LOOKUP);
    g_productionStatusU64Lookup =
        reinterpret_cast<ProductionStatusMapLookupFunction>(
            base + RVA_PRODUCTION_STATUS_U64_LOOKUP);
    g_productionStatusEventLookup =
        reinterpret_cast<ProductionStatusMapLookupFunction>(
            base + RVA_PRODUCTION_STATUS_EVENT_LOOKUP);
    g_productionStatusEventNotify =
        reinterpret_cast<ProductionStatusEventNotifyFunction>(
            base + RVA_PRODUCTION_STATUS_EVENT_NOTIFY);
    g_productionTransferFaulted.store(false, std::memory_order_release);
    g_productionTransferQuarantine.store(nullptr, std::memory_order_release);
    memset(g_productionInputQuarantine, 0,
           sizeof(g_productionInputQuarantine));
    g_productionInputQuarantineCount = 0;
    g_productionBindingMutationInProgress.store(
        false, std::memory_order_release);
    g_productionModalOpeningOwner.store(nullptr, std::memory_order_release);
    g_productionModalOpeningOwnerGeneration.store(
        0, std::memory_order_release);
    g_productionPreUpdateStage.store(0, std::memory_order_release);
    ProductionForceReleaseModalInputBlock();
    // Never start the settle clock until the first verified world context
    // activation.  There is no periodic refresh interval.
    g_productionWorldActiveSince = 0;
    g_productionTransferDeferredLogged = false;
    g_productionNaturalMorningGenerationObserved = 0;
    g_productionNaturalMorningEdgePending = 0;
    g_productionNaturalMorningSettleReady = false;
    g_productionScheduleClockWakeObserved =
        g_productionMainWorldGameClockWakeSequence.load(
            std::memory_order_acquire);
    ProductionInvalidateScheduleClock();
    g_productionTransferNotBeforeTick.store(
        GetTickCount64() + PRODUCTION_TRANSFER_SETTLE_MS,
        std::memory_order_release);
    g_productionRegistryScanRequested.store(false, std::memory_order_release);
    g_productionFaulted.store(false, std::memory_order_release);
    g_productionReady.store(true, std::memory_order_release);
    ProductionLog(
        "[PRODAUTO] seq=%llu txn=0 event=install result=PASS "
        "reason=none version=v1.1.29 "
        "schema=1 recipe_rows=%zu machine_types=29 "
        "recipe_catalog_sha256=%s all_map_registry=1 "
        "gimmick_status_vtables=primary_secondary_rtti "
        "native_ui_hooks=0 renderer_hooks=0 native_input_code_hook=0 "
        "game_iat_api_hooks=6 directinput_factory_proxy_aw=1 "
        "directinput_target_interface=a modal_input_api_gate=1 "
        "modal_foreground_window=0 callback_local_input_mask=1 "
        "no_activate_panel=1 "
        "periodic_scan=0 periodic_registry_walk=0 on_demand_scan=1 "
        "scan_batch=%zu game_time_due_state_machine=1 "
        "resolve_bindings_per_callback=1 "
        "steady_no_due_virtual_queries=0 "
        "clock_snapshot=atomic_main_world_hook "
        "retry_backoff=exponential retry_max_game_ticks=108000 "
        "panel_trigger=f3_or_dpad_right "
        "target_source=native_selected_sensor_plus_verified_logical_owner_chain "
        "raw_pointer_cache=0 native_save_writes=1 "
        "output_transfer=rank_aware_native_finalize multi_stack_delivery=1 "
        "input_push=1 input_source=bound_chest "
        "recipe_policy=lowest_chest_slot_then_recipe_id_ascending "
        "cycle=output_first_then_input_next_safe_refresh "
        "native_player_push=0 player_inventory_calls=0 "
        "push_player_coupled=1 pop_clears_before_delivery=1 "
        "chest_commit_void_clear_then_rebuild=0 "
        "chest_delivery=count_adjust_or_slot_publish "
        "inproc_native_notify=1 pre_native_update_phase=1 "
        "transfer_budget=1 due_batch_max=%u due_batch_budget_us=%lld "
        "post_scan_settle_ms=%llu "
        "binding_settle_ms=%llu post_mutation_callback_gate=1 "
        "binding_mutation_pending_gate=1 load_generation_gate=1 "
        "due_snapshot_cache=callback_local transaction_cache=fresh "
        "live_write_revalidation=1 output_chest_live_component_required=0 "
        "machine_component_gate=identity_plus_soft_roundtrip "
        "registry_index_cache=%zu "
        "transient_retry_game_ticks=%lld "
        "modal_input_release_paths=centralized\n",
        village_qol::production_automation::kProcessingRecipeTableCount,
        PRODUCTION_RECIPE_CATALOG_SHA256,
        PRODUCTION_SCAN_BATCH_OBJECTS,
        PRODUCTION_DUE_BATCH_MAX_PER_CALLBACK,
        static_cast<long long>(PRODUCTION_DUE_BATCH_BUDGET_US),
        static_cast<unsigned long long>(PRODUCTION_POST_SCAN_SETTLE_MS),
        static_cast<unsigned long long>(PRODUCTION_TRANSFER_SETTLE_MS),
        PRODUCTION_REGISTRY_INDEX_CACHE_CAPACITY,
        static_cast<long long>(
            PRODUCTION_TRANSIENT_OUTPUT_RETRY_GAME_TICKS));
    return true;
}


