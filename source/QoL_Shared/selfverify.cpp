// ================================================
// 作者：PHJ&消失的清风
// 项目：Village in the Shade QoL MOD Pack
// 转载或分享时请注明出处
// ================================================
// selfverify.cpp —— DLL 自校验实现

#include "selfverify.h"
#include "logging.h"
#include <bcrypt.h>
#include <string>

#pragma comment(lib, "bcrypt.lib")

// 全局签名实例——每个 MOD DLL 链接自己的副本
// embed_hash.py 在编译后的 DLL 中搜索 magic bytes 来定位此结构
__declspec(allocate(".qolhash"))
QOL_HASH_SIGNATURE g_selfHash = {
    { 'Q', 'O', 'L', 0xDE },   // magic
    { 0 },                       // sha256（编译后由 embed_hash.py 填入）
    { 0 }                        // reserved
};

// 节区声明（MSVC 链接器需要）
#pragma section(".qolhash", read, write)

static bool ComputeFileHash(const wchar_t* dllPath, uint8_t outHash[32]) {
    HANDLE hFile = CreateFileW(dllPath, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    // 读取整个 DLL 文件
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0) {
        CloseHandle(hFile);
        return false;
    }

    // 限制最大 16MB（MOD DLL 远小于此）
    if (fileSize.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(hFile);
        return false;
    }

    DWORD fileSize32 = (DWORD)fileSize.QuadPart;
    uint8_t* buf = (uint8_t*)VirtualAlloc(nullptr, fileSize32, MEM_COMMIT, PAGE_READWRITE);
    if (!buf) { CloseHandle(hFile); return false; }

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buf, fileSize32, &bytesRead, nullptr) || bytesRead != fileSize32) {
        VirtualFree(buf, 0, MEM_RELEASE);
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);

    // 定位签名段在文件中的偏移，将签名段字节置零后再计算哈希
    // 这样哈希覆盖 DLL 全部内容（签名段除外），嵌入哈希不影响校验值
    size_t sigOffset = (size_t)0;
    bool found = false;
    const uint8_t magic[4] = { 'Q', 'O', 'L', 0xDE };
    for (size_t i = 0; i + sizeof(QOL_HASH_SIGNATURE) <= fileSize32; ++i) {
        if (buf[i] == magic[0] && buf[i+1] == magic[1] &&
            buf[i+2] == magic[2] && buf[i+3] == magic[3]) {
            sigOffset = i;
            found = true;
            break;
        }
    }

    if (found && sigOffset + sizeof(QOL_HASH_SIGNATURE) <= fileSize32) {
        memset(buf + sigOffset, 0, sizeof(QOL_HASH_SIGNATURE));
    }

    // BCrypt SHA-256
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0) {
            if (BCryptHashData(hHash, buf, fileSize32, 0) == 0) {
                if (BCryptFinishHash(hHash, outHash, 32, 0) == 0) {
                    ok = true;
                }
            }
            BCryptDestroyHash(hHash);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }

    VirtualFree(buf, 0, MEM_RELEASE);
    return ok;
}

static void HashToHex(const uint8_t hash[32], char out[65]) {
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 32; ++i) {
        out[i * 2]     = hex[hash[i] >> 4];
        out[i * 2 + 1] = hex[hash[i] & 0x0F];
    }
    out[64] = '\0';
}

extern "C" bool SelfVerifyInit(const char* feature) {
    // 检查是否已嵌入哈希（sha256 全零 = 尚未嵌入）
    bool hasHash = false;
    for (int i = 0; i < 32; ++i) {
        if (g_selfHash.sha256[i] != 0) { hasHash = true; break; }
    }

    if (!hasHash) {
        // 未嵌入哈希（开发阶段直接编译的 DLL），跳过校验
        Log("[%s] self-verify: no embedded hash (dev build), skip", feature);
        return true;
    }

    // 获取自身 DLL 路径
    wchar_t dllPath[MAX_PATH] = {0};
    HMODULE hSelf = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&g_selfHash, &hSelf);
    if (!hSelf || !GetModuleFileNameW(hSelf, dllPath, MAX_PATH)) {
        Log("[%s] self-verify: cannot get DLL path, skip", feature);
        return true;  // 路径获取失败不阻断（避免误杀）
    }

    uint8_t actualHash[32] = {0};
    if (!ComputeFileHash(dllPath, actualHash)) {
        Log("[%s] self-verify: hash computation failed, skip", feature);
        return true;  // 计算失败不阻断
    }

    // 比对
    if (memcmp(g_selfHash.sha256, actualHash, 32) == 0) {
        Log("[%s] self-verify: OK", feature);
        return true;
    }

    // 校验失败——DLL 被篡改
    char expected[65], actual[65];
    HashToHex(g_selfHash.sha256, expected);
    HashToHex(actualHash, actual);

    // 日志留痕（方便排查）
    Log("[%s] *** SELF-VERIFY FAILED *** DLL has been modified!", feature);
    Log("[%s]   expected SHA-256: %s", feature, expected);
    Log("[%s]   actual   SHA-256: %s", feature, actual);
    Log("[%s]   DLL path: %S", feature, dllPath);
    Log("[%s]   Feature DISABLED to prevent save corruption.", feature);

    return false;
}
