// LangHelper v3.5 - GetText Hook + DB traversal + BSS scan with strict validation
//
// v3.5 changes:
//   - Strict DB validation: mask must be power-of-2 minus 1, <= 0xFFFF, sentinel non-zero
//   - BSS scan: per-candidate SEH wrapper prevents one crash from killing entire scan
//   - Lowered TraverseHashTable/TraverseSingleDB mask cap from 0x10000000 to 0xFFFF
//   - Source tracking: separate counts for db_direct / mgr / bss in final summary
// v3.3: BSS scanner - scan 0x10D5000~0x10D5D00 for hash-table DB pointers
// v3.2: Restored JA protection table, GetText counter, manager probe
//
// Layer 1: GetText Hook -- intercept all text going through GetText, T2S + missing translation
// Layer 2: DB hash table traversal -- replace traditional text in-place in ALL DB containers
//
// Manager structure (discovered via reverse engineering):
//   [RVA_MGR_PTR] = manager_ptr
//   [manager_ptr + 0x00] = array_base_ptr
//   array_base_ptr[index] = entry_ptr (one per category)
//   [entry_ptr + 0x18] = container (hash table: +0x48 sentinel, +0x58 buckets, +0x70 mask)
//   GetText uses [RVA_DB_PTR] directly, which is one specific container
//
// Author: PHJ& Qing Shi De Qing Feng, zhuan zai huo fen xiang shi qing zhu ming chu chu.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <windows.h>
#include <psapi.h>

#include "t2s_table.h"
#include "ja_to_tc_fixup.h"

// ============================================================
// Module base & key RVA offsets (v1.08.1 build 24969282)
// ============================================================

static uintptr_t g_base = 0;
static constexpr uintptr_t RVA_DB_PTR   = 0x010D59F0;  // qword: text database pointer (used by GetText)
static constexpr uintptr_t RVA_MGR_PTR  = 0x010D59E8;  // qword: multi-category text manager pointer
static constexpr uintptr_t RVA_LANG_ID  = 0x010D59E0;  // dword: current language ID

// ============================================================
// Memory helpers
// ============================================================

static bool WriteMem(void* dst, const void* src, size_t size) {
    DWORD old;
    if (!VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &old))
        return false;
    memcpy(dst, src, size);
    VirtualProtect(dst, size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, size);
    return true;
}

static bool WriteData(void* dst, const void* src, size_t size) {
    DWORD old;
    if (!VirtualProtect(dst, size, PAGE_READWRITE, &old))
        return false;
    memcpy(dst, src, size);
    VirtualProtect(dst, size, old, &old);
    return true;
}

static bool IsReadablePtr(const void* ptr) {
    if (!ptr) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(ptr, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    DWORD prot = mbi.Protect & 0xFF;
    return (prot & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                    PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

// ============================================================
// Logging
// ============================================================

#define LANGHELPER_LOGGING  // Diagnostic version: keep logging enabled

#ifdef LANGHELPER_LOGGING
#include "../QoL_Shared/logging.h"
#else
#define LogOpen(x) ((void)0)
#define Log(...)   ((void)0)
#define LogClose() ((void)0)
#endif

#include "../QoL_Shared/aobscan.h"

// ============================================================
// JA text protection table (restored from v1)
// ============================================================

static std::unordered_map<std::string, std::string> g_jaToSc;
static std::unordered_set<std::string> g_jaTextSet;

static void BuildJaToScMap() {
    for (int i = 0; i < g_jaToTcCount; ++i) {
        std::string ja = g_jaToTcFixups[i].ja_text;
        std::string tc = g_jaToTcFixups[i].tc_text;
        std::string sc = T2S_Convert(tc.c_str());

        g_jaToSc[ja] = sc;
        g_jaTextSet.insert(ja);

        std::string tagged = "<lang JP>" + ja + "</lang>";
        g_jaToSc[tagged] = sc;
        g_jaTextSet.insert(tagged);
    }
    Log("LangHelper: JA protection table built: %zu entries", g_jaToSc.size());
}

// ============================================================
// Layer 1: GetText Hook - with call counter
// ============================================================

static std::atomic<uint64_t> g_getTextCalls{0};
static std::atomic<uint64_t> g_getTextConverted{0};

// Hook infrastructure
static void* g_trampPage = nullptr;
static uintptr_t g_origGetText = 0;

typedef const char* (*GetTextFunc)(uint64_t key);
static GetTextFunc g_origGetTextPtr = nullptr;

// Conversion result cache
static std::unordered_map<std::string, const char*> g_cache;
static std::vector<char*> g_owned;

static const char* CacheString(const std::string& s) {
    auto it = g_cache.find(s);
    if (it != g_cache.end()) return it->second;
    char* copy = new char[s.size() + 1];
    memcpy(copy, s.data(), s.size());
    copy[s.size()] = '\0';
    g_cache[s] = copy;
    g_owned.push_back(copy);
    return copy;
}

static const char* DetourGetText(uint64_t key) {
    const char* result = g_origGetTextPtr(key);
    g_getTextCalls.fetch_add(1, std::memory_order_relaxed);

    if (!result || result[0] == '\0') return result;

    // 1. Check JA->SC map (missing TW entries with JA fallback)
    auto mit = g_jaToSc.find(result);
    if (mit != g_jaToSc.end()) {
        g_getTextConverted.fetch_add(1, std::memory_order_relaxed);
        return CacheString(mit->second);
    }

    // 2. If contains CJK chars, convert traditional to simplified
    if (strpbrk(result, "\xE4\xE5\xE6\xE7\xE8\xE9")) {
        auto it = g_cache.find(result);
        if (it != g_cache.end()) return it->second;

        std::string sc = T2S_Convert(result);
        if (sc != result) {
            g_getTextConverted.fetch_add(1, std::memory_order_relaxed);
            return CacheString(sc);
        }
        return result;
    }

    return result;
}

#pragma pack(push, 1)
struct JmpAbs14 {
    uint8_t ff25[6] = {0xFF, 0x25, 0x00, 0x00, 0x00, 0x00};
    uintptr_t imm64 = 0;
};
#pragma pack(pop)
static_assert(sizeof(JmpAbs14) == 14, "JmpAbs14 must be 14 bytes");

static uintptr_t ScanAOB(const char* aobText) {
    return qol::ScanModuleAOB(nullptr, aobText);
}

static bool InstallGetTextHook() {
    const char* aob = "48 8B 03 48 8B 44 C8 30 48 83 C4 30 5B C3";
    uintptr_t aobAddr = ScanAOB(aob);
    if (!aobAddr) {
        Log("LangHelper: AOB scan failed for GetText epilogue");
        return false;
    }

    g_origGetText = aobAddr - 0x4D;
    const uint8_t* funcBytes = (const uint8_t*)g_origGetText;

    const uint8_t expectedStart[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0xD9, 0x48, 0x8B, 0x0D};
    if (memcmp(funcBytes, expectedStart, sizeof(expectedStart)) != 0) {
        Log("LangHelper: Function start verification failed at %p", (void*)g_origGetText);
        return false;
    }

    int32_t disp32 = *(const int32_t*)(funcBytes + 0x0C);
    uintptr_t ripAfter = g_origGetText + 0x10;
    uintptr_t target = ripAfter + disp32;
    uintptr_t expectedTarget = g_base + RVA_DB_PTR;
    if (target != expectedTarget) {
        Log("LangHelper: DB ptr verification failed: %p != %p", (void*)target, (void*)expectedTarget);
        return false;
    }

    Log("LangHelper: GetText at %p, DB ptr at %p", (void*)g_origGetText, (void*)target);

    g_trampPage = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_trampPage) { Log("LangHelper: Failed alloc trampoline"); return false; }

    uint8_t* t = (uint8_t*)g_trampPage;
    constexpr size_t kPatchSize = 16;

    memcpy(t, funcBytes, 9);

    int32_t origDisp = *(const int32_t*)(funcBytes + 0x0C);
    uintptr_t absTarget = g_origGetText + 0x10 + (int64_t)origDisp;
    t[9]  = 0x48; t[10] = 0xB8;
    *(uintptr_t*)(t + 11) = absTarget;
    t[19] = 0x48; t[20] = 0x8B; t[21] = 0x08;
    t[22] = 0xFF; t[23] = 0x25; t[24] = 0x00; t[25] = 0x00; t[26] = 0x00; t[27] = 0x00;
    *(uintptr_t*)(t + 28) = g_origGetText + kPatchSize;

    DWORD oldProt;
    VirtualProtect(g_trampPage, 4096, PAGE_EXECUTE_READ, &oldProt);

    g_origGetTextPtr = (GetTextFunc)g_trampPage;

    uint8_t patch[16];
    JmpAbs14 jmp;
    jmp.imm64 = (uintptr_t)&DetourGetText;
    memcpy(patch, &jmp, sizeof(jmp));
    patch[14] = 0x90; patch[15] = 0x90;

    if (!WriteMem((void*)g_origGetText, patch, 16)) {
        Log("LangHelper: Failed write jump at %p", (void*)g_origGetText);
        return false;
    }

    Log("LangHelper: Hook installed at %p -> %p (tramp=%p)",
        (void*)g_origGetText, (void*)&DetourGetText, g_trampPage);
    return true;
}

static uint8_t g_origBytes[16];
static bool g_hooked = false;

static void RestoreGetText() {
    if (g_hooked && g_origGetText) {
        WriteMem((void*)g_origGetText, g_origBytes, 16);
        g_hooked = false;
        Log("LangHelper: GetText hook restored");
    }
    if (g_trampPage) {
        VirtualFree(g_trampPage, 0, MEM_RELEASE);
        g_trampPage = nullptr;
    }
}

// ============================================================
// Layer 2: DB hash table traversal (now with manager probing)
// ============================================================

static std::unordered_set<const char*> g_replacedPtrs;

static int g_replacedFromDB = 0;
static int g_replacedFromMgr = 0;
static int g_replacedFromBSS = 0;
static void OnContainer(uintptr_t container, void* ctx);
static void OnSubEntry(uintptr_t textArray, void* ctx);

static void ReplaceTextInPlace(char* tcText) {
    if (!tcText || tcText[0] == '\0') return;
    if (g_replacedPtrs.count(tcText)) return;

    if (!IsReadablePtr(tcText) || !IsReadablePtr(tcText + 1)) return;
    size_t origLen = strnlen(tcText, 4096);
    if (origLen >= 4096) return;
    if (!IsReadablePtr(tcText + origLen)) return;

    g_replacedPtrs.insert(tcText);

    std::string tcStr(tcText, origLen);

    // JA text protection: skip JA fallback texts to avoid corrupting shared JA memory
    if (g_jaTextSet.count(tcStr)) {
        Log("LangHelper: skipping JA fallback text (protected): \"%.60s\"", tcText);
        return;
    }

    std::string simplified = T2S_Convert(tcStr.c_str());
    if (simplified == tcStr) return;

    size_t simpLen = simplified.size();
    if (simpLen > origLen) {
        Log("WARNING: simplified longer than original! orig=%zu simp=%zu", origLen, simpLen);
        return;
    }
    if (!WriteData(tcText, simplified.c_str(), simpLen)) return;
    if (simpLen < origLen) tcText[simpLen] = '\0';
}

static void TraverseHashTable(uintptr_t bucketArr, uintptr_t mask, uintptr_t sentinel,
                              void (*onEntry)(uintptr_t payload, void* ctx), void* ctx,
                              const char* tableName) {
    if (!bucketArr || !IsReadablePtr((const void*)bucketArr)) return;
    if (mask == 0 || mask > 0xFFFF) return;

    uint64_t numBuckets = mask + 1;
    int total = 0;
    const int MAX_ENTRIES = 50000;
    ULONGLONG startTick = GetTickCount64();
    const ULONGLONG TIMEOUT_MS = 3000;

    for (uint64_t i = 0; i < numBuckets && i < 0x10000000; ++i) {
        if (total >= MAX_ENTRIES) {
            Log("LangHelper: %s: entry cap hit (%d), truncating", tableName, MAX_ENTRIES);
            break;
        }
        if ((i & 0xFF) == 0 && GetTickCount64() - startTick > TIMEOUT_MS) {
            Log("LangHelper: %s: timeout (%llums, %d entries), truncating", tableName, GetTickCount64() - startTick, total);
            break;
        }
        uintptr_t bucketAddr = bucketArr + i * 16;
        if (!IsReadablePtr((const void*)bucketAddr)) break;

        uintptr_t anchor = *(uintptr_t*)(bucketAddr + 0x00);
        uintptr_t first  = *(uintptr_t*)(bucketAddr + 0x08);
        if (!first || first == anchor || first == sentinel) continue;

        uintptr_t entry = first;
        for (int safety = 0; safety < 100000 && entry && entry != anchor; ++safety) {
            if (!IsReadablePtr((const void*)(entry + 0x18))) break;
            uintptr_t payload = *(uintptr_t*)(entry + 0x18);
            if (payload && onEntry) onEntry(payload, ctx);
            total++;

            uintptr_t next = *(uintptr_t*)(entry + 0x08);
            if (!next || next == anchor) break;
            entry = next;
        }
    }
    Log("LangHelper: %s: %d entries traversed", tableName, total);
}

static void OnContainer(uintptr_t container, void* ctx) {
    int* langId = (int*)ctx;
    if (!container || !IsReadablePtr((const void*)container)) return;
    if (!IsReadablePtr((const void*)(container + 0x58))) return;

    uintptr_t subSentinel = *(uintptr_t*)(container + 0x60);
    uintptr_t subBuckets  = *(uintptr_t*)(container + 0x70);
    uintptr_t subMask     = *(uintptr_t*)(container + 0x88);
    if (!subBuckets || !IsReadablePtr((const void*)subBuckets)) return;
    if (subMask == 0 || subMask > 0xFFFF) return;  // strict cap: max 65535 buckets

    TraverseHashTable(subBuckets, subMask, subSentinel, OnSubEntry, langId, "sub");
}

static void OnSubEntry(uintptr_t textArray, void* ctx) {
    int* langId = (int*)ctx;
    if (!textArray || !IsReadablePtr((const void*)textArray)) return;

    uintptr_t charPtrAddr = textArray + 0x30 + (uintptr_t)(*langId) * 8;
    if (!IsReadablePtr((const void*)charPtrAddr)) return;

    uintptr_t textPtr = *(uintptr_t*)charPtrAddr;
    if (!textPtr || !IsReadablePtr((const void*)textPtr)) return;

    ReplaceTextInPlace((char*)textPtr);
}

// Traverse a single DB container (given its address directly)
static int TraverseSingleDB(uintptr_t dbPtr, int langId, const char* label) {
    if (!dbPtr || !IsReadablePtr((const void*)dbPtr)) {
        Log("LangHelper: [%s] dbPtr=%p not readable, skipping", label, (void*)dbPtr);
        return 0;
    }

    uintptr_t sentinel = *(uintptr_t*)(dbPtr + 0x48);
    uintptr_t buckets  = *(uintptr_t*)(dbPtr + 0x58);
    uintptr_t mask     = *(uintptr_t*)(dbPtr + 0x70);
    if (!buckets || !IsReadablePtr((const void*)buckets)) {
        Log("LangHelper: [%s] buckets not readable, skipping", label);
        return 0;
    }
    if (mask == 0 || mask > 0xFFFF) {
        Log("LangHelper: [%s] mask=0x%llx out of range, skipping", label, (uint64_t)mask);
        return 0;
    }

    Log("LangHelper: [%s] dbPtr=%p sentinel=%p buckets=%p mask=0x%llx (%llu buckets)",
        label, (void*)dbPtr, (void*)sentinel, (void*)buckets, (uint64_t)mask, (uint64_t)(mask+1));

    ULONGLONG dbStart = GetTickCount64();
    size_t prevReplaced = g_replacedPtrs.size();
    TraverseHashTable(buckets, mask, sentinel, OnContainer, &langId, label);
    int newReplaced = (int)(g_replacedPtrs.size() - prevReplaced);
    ULONGLONG dbElapsed = GetTickCount64() - dbStart;
    Log("LangHelper: [%s] replaced %d new texts (total: %zu) in %llums", label, newReplaced, g_replacedPtrs.size(), dbElapsed);
    Log("LangHelper: [%s] replaced %d new texts (total: %zu) in %llums", label, newReplaced, g_replacedPtrs.size(), dbElapsed);
    if (newReplaced > 0) {
        if (label[0] == 'd' && label[1] == 'b') g_replacedFromDB += newReplaced;
        else if (label[0] == 'm' && label[1] == 'g') g_replacedFromMgr += newReplaced;
        else if (label[0] == 'b' && label[1] == 's') g_replacedFromBSS += newReplaced;
    }
    return newReplaced;
}

// Probe the multi-category manager at RVA_MGR_PTR
static void ProbeManager(int langId) {
    uintptr_t mgrPtr = *(uintptr_t*)(g_base + RVA_MGR_PTR);
    Log("LangHelper: === Manager Probe (0x10D59E8) ===");
    Log("LangHelper: MGR_PTR value = %p", (void*)mgrPtr);

    if (!mgrPtr || !IsReadablePtr((const void*)mgrPtr)) {
        Log("LangHelper: Manager ptr not readable, skipping manager probe");
        return;
    }

    // [manager_ptr + 0x00] = array_base_ptr
    uintptr_t arrayBase = *(uintptr_t*)(mgrPtr);
    Log("LangHelper: Manager array_base = %p", (void*)arrayBase);

    if (!arrayBase || !IsReadablePtr((const void*)arrayBase)) {
        Log("LangHelper: Manager array_base not readable, skipping");
        return;
    }

    // Scan array entries (up to 256 categories)
    int validContainers = 0;
    int totalReplacedFromManager = 0;

    for (int cat = 0; cat < 256; cat++) {
        uintptr_t entryAddr = arrayBase + (uintptr_t)cat * 8;
        if (!IsReadablePtr((const void*)(entryAddr + 8))) break;

        uintptr_t entryPtr = *(uintptr_t*)entryAddr;
        if (!entryPtr || !IsReadablePtr((const void*)entryPtr)) {
            if (cat < 4) Log("LangHelper: cat[%d] entry=%p (null/invalid), stopping", cat, (void*)entryPtr);
            // Don't stop immediately - there might be gaps. But if 8 consecutive nulls, stop.
            bool allNull = true;
            for (int check = 0; check < 8 && (cat + check) < 256; check++) {
                uintptr_t ea = arrayBase + (uintptr_t)(cat + check) * 8;
                if (IsReadablePtr((const void*)ea)) {
                    uintptr_t ep = *(uintptr_t*)ea;
                    if (ep && IsReadablePtr((const void*)ep)) { allNull = false; break; }
                }
            }
            if (allNull) break;
            continue;
        }

        // [entry_ptr + 0x18] = container (hash table DB)
        if (!IsReadablePtr((const void*)(entryPtr + 0x20))) continue;
        uintptr_t containerPtr = *(uintptr_t*)(entryPtr + 0x18);
        if (!containerPtr || !IsReadablePtr((const void*)containerPtr)) {
            if (cat < 4) Log("LangHelper: cat[%d] entry=%p container=%p (null/invalid)",
                cat, (void*)entryPtr, (void*)containerPtr);
            continue;
        }

        // Check if container has valid hash table structure
        if (!IsReadablePtr((const void*)(containerPtr + 0x78))) continue;
        uintptr_t testMask = *(uintptr_t*)(containerPtr + 0x70);
        uintptr_t testBuckets = *(uintptr_t*)(containerPtr + 0x58);
        if (testBuckets == 0 || testMask == 0 || testMask > 0x10000000) {
            if (cat < 4) Log("LangHelper: cat[%d] container=%p invalid hash table (buckets=%p mask=0x%llx)",
                cat, (void*)containerPtr, (void*)testBuckets, (uint64_t)testMask);
            continue;
        }

        char label[32];
        snprintf(label, sizeof(label), "mgr_cat[%d]", cat);
        Log("LangHelper: cat[%d] entry=%p container=%p buckets=%p mask=0x%llx -> VALID",
            cat, (void*)entryPtr, (void*)containerPtr, (void*)testBuckets, (uint64_t)testMask);

        validContainers++;
        int replaced = TraverseSingleDB(containerPtr, langId, label);
        totalReplacedFromManager += replaced;
    }

    Log("LangHelper: Manager probe done: %d valid containers, %d new texts replaced from manager",
        validContainers, totalReplacedFromManager);

    // Check: does the manager's cat[0] container == the DB at 0x10D59F0?
    uintptr_t dbPtr = *(uintptr_t*)(g_base + RVA_DB_PTR);
    uintptr_t mgrCat0Entry = *(uintptr_t*)(arrayBase);
    uintptr_t mgrCat0Container = 0;
    if (mgrCat0Entry && IsReadablePtr((const void*)(mgrCat0Entry + 0x20))) {
        mgrCat0Container = *(uintptr_t*)(mgrCat0Entry + 0x18);
    }
    Log("LangHelper: DB_PTR(0x10D59F0)=%p vs MGR_cat[0]_container=%p -> %s",
        (void*)dbPtr, (void*)mgrCat0Container,
        (dbPtr == mgrCat0Container) ? "SAME" : "DIFFERENT");
}

// ============================================================
// BSS DB Scanner (v3.3)
// Scan BSS region 0x10D5000~0x10D5D00 for additional DB pointers
// Each qword in range is tested as a potential hash-table DB container
// ============================================================

static bool IsValidDBContainer(uintptr_t ptr) {
    if (!ptr || !IsReadablePtr((const void*)ptr)) return false;
    if (!IsReadablePtr((const void*)(ptr + 0x78))) return false;

    uintptr_t sentinel = *(uintptr_t*)(ptr + 0x48);
    uintptr_t buckets  = *(uintptr_t*)(ptr + 0x58);
    uintptr_t mask     = *(uintptr_t*)(ptr + 0x70);

    // Buckets must be a valid readable pointer
    if (buckets == 0 || !IsReadablePtr((const void*)buckets)) return false;

    // Mask must be power-of-2 minus 1 (valid hash table: 0x1, 0x3, 0x5, 0x7, 0xF, 0x1F, ...)
    // Also cap at 0xFFFF (65536 buckets max) -- real game DBs use 0x5 (6 buckets) or 0x1FF (512)
    if (mask == 0 || mask > 0xFFFF) return false;
    if ((mask & (mask + 1)) != 0) return false;  // not 2^N - 1

    // Sentinel must be non-zero and readable (it is the list head anchor)
    if (sentinel == 0) return false;
    if (!IsReadablePtr((const void*)sentinel)) return false;

    // Extra sanity: sentinel should point to a valid memory region near buckets
    // (in practice sentinel is often buckets - 8 or a nearby allocation)
    return true;
}

// Helper: safely read a pointer, returns 0 on crash (SEH-compliant: no C++ objects with destructors)
static uintptr_t SafeReadPointer(uintptr_t addr) {
    __try {
        return *(uintptr_t*)addr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// Helper: safely traverse a DB, returns 0 on crash
static int SafeTraverseDB(uintptr_t dbPtr, int langId, const char* label) {
    __try {
        return TraverseSingleDB(dbPtr, langId, label);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Log("LangHelper: [%s] SEH crash during traversal, skipping", label);
        return 0;
    }
}

static void ScanBSSForDBs(int langId) {
    Log("LangHelper: === BSS DB Scan (0x10D5000~0x10D5D00) ===");

    // Collect known DB pointers to skip
    std::unordered_set<uintptr_t> knownDBs;

    uintptr_t dbPtr = *(uintptr_t*)(g_base + RVA_DB_PTR);
    if (dbPtr) knownDBs.insert(dbPtr);

    uintptr_t mgrPtr = *(uintptr_t*)(g_base + RVA_MGR_PTR);
    if (mgrPtr && IsReadablePtr((const void*)mgrPtr)) {
        uintptr_t arrayBase = *(uintptr_t*)(mgrPtr);
        if (arrayBase && IsReadablePtr((const void*)arrayBase)) {
            for (int cat = 0; cat < 256; cat++) {
                uintptr_t entryAddr = arrayBase + (uintptr_t)cat * 8;
                if (!IsReadablePtr((const void*)(entryAddr + 8))) break;
                uintptr_t entryPtr = *(uintptr_t*)entryAddr;
                if (!entryPtr || !IsReadablePtr((const void*)(entryPtr + 0x20))) continue;
                uintptr_t containerPtr = *(uintptr_t*)(entryPtr + 0x18);
                if (containerPtr) knownDBs.insert(containerPtr);
            }
        }
    }

    Log("LangHelper: BSS scan: %zu known DBs to skip", knownDBs.size());

    // Scan BSS region: 0x10D5000 ~ 0x10D5D00, step 8 bytes (qword pointers)
    static constexpr uintptr_t BSS_SCAN_START = 0x010D5000;
    static constexpr uintptr_t BSS_SCAN_END   = 0x010D5D00;

    int newDBsFound = 0;
    int totalNewReplaced = 0;

    for (uintptr_t rva = BSS_SCAN_START; rva < BSS_SCAN_END; rva += 8) {
        uintptr_t addr = g_base + rva;
        if (!IsReadablePtr((const void*)addr)) break;

        uintptr_t candidate = 0;
        candidate = SafeReadPointer(addr);
        if (!candidate || candidate < 0x10000 || candidate > 0x7FFFFFFFFFFF) continue;

        // Skip if already known
        if (knownDBs.count(candidate)) continue;

        // Test if it is a valid DB container (strict validation)
        if (!IsValidDBContainer(candidate)) continue;

        // Found a new DB! Traverse it inside SEH to prevent one crash from killing scan
        char label[48];
        snprintf(label, sizeof(label), "bss_0x%llX", (uint64_t)rva);
        Log("LangHelper: [BSS] NEW DB at RVA 0x%llX -> ptr=%p (sentinel=%p buckets=%p mask=0x%llx)",
            (uint64_t)rva, (void*)candidate,
            (void*)(*(uintptr_t*)(candidate + 0x48)),
            (void*)(*(uintptr_t*)(candidate + 0x58)),
            (uint64_t)(*(uintptr_t*)(candidate + 0x70)));

        knownDBs.insert(candidate);

        int replaced = 0;
        replaced = SafeTraverseDB(candidate, langId, label);
        if (replaced > 0) {
            newDBsFound++;
            totalNewReplaced += replaced;
        }
    }

    Log("LangHelper: BSS scan done: %d new DBs found, %d new texts replaced from BSS",
        newDBsFound, totalNewReplaced);
}

// Main entry: probe manager + traverse all DBs
static void ReplaceAllTextInDB() {
    int langId = *(int32_t*)(g_base + RVA_LANG_ID);
    Log("LangHelper: lang_id=%d", langId);
    if (langId < 0 || langId > 20) { Log("LangHelper: lang_id out of range, aborting"); return; }

    // 1. Traverse the original DB (0x10D59F0) -- same as v3.1
    uintptr_t dbPtr = *(uintptr_t*)(g_base + RVA_DB_PTR);
    Log("LangHelper: === DB_PTR (0x10D59F0) ===");
    Log("LangHelper: DB_PTR value = %p", (void*)dbPtr);
    if (dbPtr && IsReadablePtr((const void*)dbPtr)) {
        TraverseSingleDB(dbPtr, langId, "db_direct");
    } else {
        Log("LangHelper: DB_PTR not ready (%p)", (void*)dbPtr);
    }

    // 2. Probe the multi-category manager (0x10D59E8)
    ProbeManager(langId);

    // 3. Scan BSS for other DB structures
    ScanBSSForDBs(langId);

    Log("LangHelper: === Summary ===");
    Log("LangHelper: Total unique texts replaced: %zu (db_direct=%d mgr=%d bss=%d)",
        g_replacedPtrs.size(), g_replacedFromDB, g_replacedFromMgr, g_replacedFromBSS);
    Log("LangHelper: GetText calls so far: %llu, converted: %llu",
        (unsigned long long)g_getTextCalls.load(), (unsigned long long)g_getTextConverted.load());
}

// ============================================================
// DLL entry
// ============================================================

static bool g_initialized = false;
static bool g_dbThreadLaunched = false;
static int g_tickCount = 0;
static int g_prevLangId = -1;
static int g_langStableCount = 0;

extern "C" __declspec(dllexport) void mod_init(void) {
    LogOpen("langhelper");

    MODULEINFO modInfo;
    GetModuleInformation(GetCurrentProcess(), GetModuleHandleA(nullptr), &modInfo, sizeof(modInfo));
    g_base = (uintptr_t)modInfo.lpBaseOfDll;

    Log("LangHelper v3.5: mod init starting... base=%p", (void*)g_base);

    BuildJaToScMap();

    if (InstallGetTextHook()) {
        memcpy(g_origBytes, (const void*)g_origGetText, 16);
        g_hooked = true;
    } else {
        Log("LangHelper: Hook installation FAILED, plugin disabled");
    }

    g_initialized = true;
    Log("LangHelper v3.5: mod_init complete");
}

extern "C" __declspec(dllexport) void mod_tick(void) {
    if (!g_dbThreadLaunched) {
        g_tickCount++;

        int curLangId = *(int32_t*)(g_base + RVA_LANG_ID);
        if (curLangId == g_prevLangId) {
            g_langStableCount++;
        } else {
            g_prevLangId = curLangId;
            g_langStableCount = 1;
        }

        if (g_langStableCount >= 30 && g_tickCount >= 10) {
            uintptr_t dbPtr = *(uintptr_t*)(g_base + RVA_DB_PTR);
            if (dbPtr && IsReadablePtr((const void*)dbPtr)) {
                g_dbThreadLaunched = true;
                Log("LangHelper: lang_id=%d stable, launching DB replace thread (tick %d)...",
                    curLangId, g_tickCount);
                std::thread([]() {
                    __try {
                        ReplaceAllTextInDB();
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        Log("LangHelper: DB replace thread crashed (SEH caught)");
                    }
                    Log("LangHelper: DB replace thread finished. %zu texts replaced. GetText calls: %llu",
                        g_replacedPtrs.size(), (unsigned long long)g_getTextCalls.load());
                }).detach();
            }
        }
    }

    // Log GetText call count periodically (every 600 ticks ~ 10 seconds)
    static int logCounter = 0;
    if (g_hooked && ++logCounter >= 600) {
        logCounter = 0;
        Log("LangHelper: GetText tick report: calls=%llu converted=%llu",
            (unsigned long long)g_getTextCalls.load(),
            (unsigned long long)g_getTextConverted.load());
    }
}

extern "C" __declspec(dllexport) void unload(void) {
    Log("LangHelper v3.5: unload starting...");

    RestoreGetText();

    for (char* p : g_owned) delete[] p;
    g_owned.clear();
    g_cache.clear();
    g_jaToSc.clear();
    g_jaTextSet.clear();
    g_replacedPtrs.clear();

    g_initialized = false;
    g_dbThreadLaunched = false;
    Log("LangHelper v3.5: unload complete. Final GetText stats: calls=%llu converted=%llu",
        (unsigned long long)g_getTextCalls.load(), (unsigned long long)g_getTextConverted.load());
    LogClose();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}
