// hotkey.cpp —— 共享热键运行时读取（文件热读 + VK 解析）
#include "hotkey.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

// ------------------------------------------------------------
// 键描述 → VK 映射（兼容 ModManager 面板既有文案）
// ------------------------------------------------------------
static WORD ParseVkName(const char* s) {
    if (!s || !*s) return VK_UNDEFINED;

    // 单字符：按字符值（'4' = 0x34 → VK 4）
    if (s[1] == '\0') {
        char c = s[0];
        if (c >= '0' && c <= '9') return (WORD)c;
        if (c >= 'A' && c <= 'Z') return (WORD)c;
        // 其他单字符（如 ',' 等）不映射
        return VK_UNDEFINED;
    }

    // 常见别名（不区分大小写）
    struct Alias { const char* name; WORD vk; };
    static const Alias kAliases[] = {
        { "enter",      VK_RETURN  },
        { "return",     VK_RETURN  },
        { "space",      VK_SPACE   },
        { "spacebar",   VK_SPACE   },
        { "tab",        VK_TAB     },
        { "esc",        VK_ESCAPE  },
        { "escape",     VK_ESCAPE  },
        { "backspace",  VK_BACK    },
        { "del",        VK_DELETE  },
        { "delete",     VK_DELETE  },
        { "ins",        VK_INSERT  },
        { "insert",     VK_INSERT  },
        { "home",       VK_HOME    },
        { "end",        VK_END     },
        { "pgup",       VK_PRIOR   },
        { "pageup",     VK_PRIOR   },
        { "pgdn",       VK_NEXT    },
        { "pagedown",   VK_NEXT    },
        { "up",         VK_UP      },
        { "down",       VK_DOWN    },
        { "left",       VK_LEFT    },
        { "right",      VK_RIGHT   },
        { "capslock",   VK_CAPITAL },
        { "lshift",     VK_LSHIFT  },
        { "rshift",     VK_RSHIFT  },
        { "lctrl",      VK_LCONTROL},
        { "rctrl",      VK_RCONTROL},
        { "lalt",       VK_LMENU   },
        { "ralt",       VK_RMENU   },
        { "shift",      VK_SHIFT   },
        { "ctrl",       VK_CONTROL },
        { "control",    VK_CONTROL },
        { "alt",        VK_MENU    },
        // 手柄方向键：不映射到键盘 VK（各 MOD 自行处理手柄逻辑）
        { "d-pad up",   VK_UNDEFINED },
        { "d-pad down", VK_UNDEFINED },
        { "d-pad left", VK_UNDEFINED },
        { "d-pad right", VK_UNDEFINED },
        { "dpad up",    VK_UNDEFINED },
        { "dpad down",  VK_UNDEFINED },
        { "dpad left",  VK_UNDEFINED },
        { "dpad right", VK_UNDEFINED },
    };
    for (size_t i = 0; i < sizeof(kAliases) / sizeof(kAliases[0]); ++i) {
        if (_stricmp(s, kAliases[i].name) == 0) return kAliases[i].vk;
    }

    // VK_ 前缀：直接转换数字
    if (_strnicmp(s, "VK_", 3) == 0) {
        const char* num = s + 3;
        char* end = nullptr;
        long v = strtol(num, &end, 0);  // 支持 0x 前缀
        if (end && *end == '\0' && v > 0 && v <= 0xFF) return (WORD)v;
        return VK_UNDEFINED;
    }

    // F 键：F1..F24
    if (s[0] == 'F' || s[0] == 'f') {
        char* end = nullptr;
        long n = strtol(s + 1, &end, 10);
        if (end && *end == '\0' && n >= 1 && n <= 24)
            return (WORD)(VK_F1 + (n - 1));
    }

    return VK_UNDEFINED;
}

// ------------------------------------------------------------
// 文件修改时间（FILETIME → uint64）
// ------------------------------------------------------------
static uint64_t FileMtime(const wchar_t* path) {
    WIN32_FILE_ATTRIBUTE_DATA info = {};
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &info))
        return 0;
    return ((uint64_t)info.ftLastWriteTime.dwHighDateTime << 32)
         | info.ftLastWriteTime.dwLowDateTime;
}

// ------------------------------------------------------------
// 按 feature 匹配行（大小写不敏感），返回该行 key 部分
// ------------------------------------------------------------
static bool FindFeatureLine(const char* content, const char* feature,
                            std::string& outKey) {
    size_t pos = 0;
    size_t len = strlen(content);
    while (pos <= len) {
        size_t eol = strchr(content + pos, '\n') ?
                     (strchr(content + pos, '\n') - content) : len;
        std::string line(content + pos, eol - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string feat = line.substr(0, eq);
            // 去掉首尾空白
            size_t b = feat.find_first_not_of(" \t\r\n");
            size_t e = feat.find_last_not_of(" \t\r\n");
            feat = (b == std::string::npos) ? "" : feat.substr(b, e - b + 1);
            if (!feature || !*feature || _stricmp(feat.c_str(), feature) == 0) {
                outKey = line.substr(eq + 1);
                // 去 key 首尾空白
                size_t kb = outKey.find_first_not_of(" \t\r\n");
                size_t ke = outKey.find_last_not_of(" \t\r\n");
                outKey = (kb == std::string::npos) ? "" : outKey.substr(kb, ke - kb + 1);
                return true;
            }
        }
        if (eol >= len) break;
        pos = eol + 1;
    }
    return false;
}

// ------------------------------------------------------------
// 解析 key 字符串（逗号分隔）到集合
// ------------------------------------------------------------
static void ParseKeysInto(QolHotKeys* hk, const std::string& key) {
    hk->count = 0;
    hk->keyboardCount = 0;
    size_t pos = 0;
    while (pos <= key.size() && hk->count < 4) {
        size_t comma = key.find(',', pos);
        if (comma == std::string::npos) comma = key.size();
        std::string part = key.substr(pos, comma - pos);
        // trim
        size_t b = part.find_first_not_of(" \t\r\n");
        size_t e = part.find_last_not_of(" \t\r\n");
        if (b != std::string::npos) {
            part = part.substr(b, e - b + 1);
            if (part.size() >= sizeof(hk->raw[0])) part.resize(sizeof(hk->raw[0]) - 1);
            strcpy_s(hk->raw[hk->count], part.c_str());
            WORD v = ParseVkName(part.c_str());
            hk->vk[hk->count] = v;
            if (v != VK_UNDEFINED) hk->keyboardCount++;
            hk->count++;
        }
        if (comma >= key.size()) break;
        pos = comma + 1;
    }
}

// ------------------------------------------------------------
// 实现
// ------------------------------------------------------------
void QolHotKeysInit(QolHotKeys* hk, const char* feature) {
    if (!hk) return;
    memset(hk, 0, sizeof(*hk));
    if (feature) strncpy_s(hk->feature, feature, sizeof(hk->feature) - 1);
    for (int i = 0; i < 4; ++i) hk->vk[i] = VK_UNDEFINED;
    hk->lastMtime = 0;
    QolHotKeysReloadNow(hk);
}

void QolHotKeysSetDefault(QolHotKeys* hk, const char* keyCsv) {
    if (!hk || !keyCsv) return;
    ParseKeysInto(hk, keyCsv);
}

int QolHotKeysReloadNow(QolHotKeys* hk) {
    if (!hk) return 0;
    const wchar_t* path = L"qol_hotkeys.txt"; // 游戏根目录
    uint64_t mtime = FileMtime(path);
    if (mtime == 0) {
        hk->hasEntry = 0;  // 文件不存在
        return 0;
    }
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"r") != 0 || !f) return 0;
    std::string content;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) content.append(buf, n);
    fclose(f);

    std::string key;
    if (!FindFeatureLine(content.c_str(), hk->feature[0] ? hk->feature : nullptr, key)) {
        // 文件存在但无本 MOD 行：保留当前值，但记录 mtime 避免每帧重读
        hk->hasEntry = 0;
        hk->lastMtime = mtime;
        return 0;
    }
    hk->hasEntry = 1;
    ParseKeysInto(hk, key);
    hk->lastMtime = mtime;
    return 1;
}

int QolHotkeyCheckReload(QolHotKeys* hk) {
    if (!hk) return 0;
    const wchar_t* path = L"qol_hotkeys.txt";
    uint64_t mtime = FileMtime(path);
    if (mtime == 0) return 0;                 // 文件不存在
    if (mtime == hk->lastMtime) return 0;     // 未变化
    return QolHotKeysReloadNow(hk);
}

WORD QolHotKeysVk(QolHotKeys* hk, int i) {
    if (!hk || i < 0 || i >= 4) return VK_UNDEFINED;
    return hk->vk[i];
}