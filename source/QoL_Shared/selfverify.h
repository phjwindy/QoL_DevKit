// ================================================
// 作者：PHJ&消失的清风
// 项目：Village in the Shade QoL MOD Pack
// 转载或分享时请注明出处
// ================================================
// selfverify.h —— DLL 自校验框架（防篡改）
//
// 原理：编译后由 embed_hash.py 计算完整 DLL 的 SHA-256，
//       将哈希写入 DLL 的预留签名段（QOL_HASH_SIGNATURE）。
//       运行时 mod_init 阶段调用 SelfVerifyInit()，重新计算
//       自身 DLL 文件的 SHA-256（排除签名段），与嵌入值比对。
//       不匹配则日志留痕并禁用功能，避免被篡改的 DLL 损坏存档。
//
// 集成方式（各 MOD 的 mod_init 开头）：
//   #include "selfverify.h"
//   extern "C" __declspec(dllexport) void mod_init(void) {
//       if (!SelfVerifyInit("myfeature")) return; // 篡改检测，不匹配则中止
//       ...原有初始化...
//   }

#pragma once
#include <windows.h>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// 预留签名段：编译时填充占位字节，embed_hash.py 定位并覆盖为实际哈希
// 64 字节 = SHA-256 的 32 字节哈希 + 32 字节零填充
// 标记字节序列用于 Python 脚本精确定位
#pragma pack(push, 1)
typedef struct QOL_HASH_SIGNATURE {
    uint8_t  magic[4];      // 'Q','O','L','\xDE'
    uint8_t  sha256[32];    // 编译后由 embed_hash.py 填入
    uint8_t  reserved[28];  // 零填充，未来扩展
} QOL_HASH_SIGNATURE;
#pragma pack(pop)

// 调用此函数在 mod_init 开头进行自校验
// feature: MOD 名称（用于日志标识）
// 返回值：true=校验通过（或首次运行未嵌入哈希），false=校验失败（DLL 被篡改）
bool SelfVerifyInit(const char* feature);

#ifdef __cplusplus
}
#endif
