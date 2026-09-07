// hotkey.h —— 共享热键运行时读取（游戏内改键链路 · 原型）
//
// 目标：让 QoL MOD 不再硬编码 GetAsyncKeyState(固定键)，
//       改为每次 mod_tick 读取共享配置 qol_hotkeys.txt（文件热读），
//       从而支持 ModManager 面板在游戏内改键、立即生效。
//
// 背景（阶段记录）：
//   - ModManager 已支持读取 qol_hotkeys.txt 覆盖默认热键（ApplyHotkeysFile）
//   - QolRegisterHotKey() 已有"写 qol_hotkeys.txt"能力（logging.cpp）
//   - 本组件补上"读 + 解析成 VK 集合"这一环，各 MOD 只需：
//       QolHotkeyLoad("chestsort");                       // mod_init 时
//       QolHotkeyIsPressed(0);                            // mod_tick 时
//   - 第三方 MOD 支持（GetAsyncKeyState 重映射钩子）留待后续阶段。
//
// 设计：
//   - 缓存 + 惰性重载：QolHotkeyCheckReload() 每 tick 调用（廉价），
//     文件 mtime 变化才真正重读，避免每帧磁盘 IO。
//   - 键描述 → VK：兼容 ModManager 面板既有文案
//     （"4"、"9"、"F6"、"F10"、"VK_xxx"、"D-pad Up"、"D-pad Down"…），
//     未知描述返回 VK_UNDEFINED（0xFF），对应槽位永不匹配。
//   - 手柄键（D-pad 等）不映射到键盘 VK，由各 MOD 保留自己的手柄逻辑；
//     本组件只负责"键盘按键"部分。
//   - 线程：仅在游戏主线程（mod_tick）调用，无锁。
#pragma once

#include <windows.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 自定义"无键盘键"占位值（0xFF 在 Win32 中无对应虚拟键）
#ifndef VK_UNDEFINED
#define VK_UNDEFINED 0xFF
#endif

// 单 MOD 热键集合（最多 4 个键盘 VK + 最多 4 个原始描述，供日志/调试）
typedef struct QolHotKeys {
    char   feature[32];          // MOD 标识（小写，如 "chestsort"）
    WORD   vk[4];               // 解析后的键盘 VK（无键盘键 = VK_UNDEFINED）
    char   raw[4][24];         // 原始描述（"4"、"F6"、"D-pad Up"…）
    int    count;              // 有效按键数（含手柄等非键盘项，便于 UI 显示）
    int    keyboardCount;      // 其中属于键盘的按键数
    int    hasEntry;           // 文件是否包含本 MOD 行（1=有，0=无）
    uint64_t lastMtime;        // 缓存文件修改时间（内部）
} QolHotKeys;

// 初始化：绑定 feature，随后每次 QolHotkeyCheckReload 惰性读文件。
// feature 非空时按 "feature=" 行匹配；为空时按第一个 "=" 行匹配（单 MOD 场景）。
// 注意：Init 只尝试读文件；若文件不存在或没有本 MOD 行，count 保持 0，
//       请随后调用 QolHotKeysSetDefault 提供默认键（推荐）。
void QolHotKeysInit(QolHotKeys* hk, const char* feature);

// 设置默认键（逗号分隔，如 "4, D-pad Up"）。
// 通常在 Init 后 count==0 时调用，或在文件不含本 MOD 行时作为回退。
void QolHotKeysSetDefault(QolHotKeys* hk, const char* keyCsv);

// 每帧调用：文件 mtime 变化则重读（廉价检查，不做磁盘读）。
// 返回 1 = 本次发生了重载。
int QolHotkeyCheckReload(QolHotKeys* hk);

// 取第 i 个键盘 VK（i 超界或该键非键盘 → VK_UNDEFINED）。
WORD QolHotKeysVk(QolHotKeys* hk, int i);

// 手动读取一次（返回 1=读到并解析）。供首次加载/强制刷新用。
int QolHotKeysReloadNow(QolHotKeys* hk);

#ifdef __cplusplus
}
#endif