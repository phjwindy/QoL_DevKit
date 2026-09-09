// logging.cpp
#include "logging.h"
#include <windows.h>
#include <string>

static FILE* g_log = nullptr;
static char  g_name[64] = "default";

extern "C" void LogOpen(const char* feature) {
    if (feature && *feature) {
        snprintf(g_name, sizeof(g_name), "%s", feature);
    }
    if (g_log) { fclose(g_log); g_log = nullptr; }
    char path[MAX_PATH] = {0};
    snprintf(path, MAX_PATH, "qol_%s.log", g_name);
    g_log = fopen(path, "w");  // 截断模式：每次启动清空旧日志
    if (g_log) {
        setvbuf(g_log, nullptr, _IONBF, 0); // 无缓冲，崩溃也不丢日志
    }
}

static void EnsureOpen() {
    if (g_log) return;
    char path[MAX_PATH] = {0};
    snprintf(path, MAX_PATH, "qol_%s.log", g_name);
    g_log = fopen(path, "a");
    if (g_log) setvbuf(g_log, nullptr, _IONBF, 0);
}

extern "C" void LogV(const char* fmt, va_list ap) {
    EnsureOpen();
    if (!g_log || !fmt) return;
    SYSTEMTIME st = {0};
    GetLocalTime(&st);
    fprintf(g_log, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    // 先格式化 UTF-8 文本到缓冲区
    char buf[8192] = {0};
    vsnprintf(buf, sizeof(buf), fmt, ap);

    // ASCII 快速路径：纯 ASCII 文本直接写，跳过 UTF-8→GBK 转换
    // 诊断日志绝大多数为纯 ASCII（指针地址、计数、状态码）
    bool pureAscii = true;
    for (const char* p = buf; *p; ++p) {
        if (static_cast<unsigned char>(*p) >= 0x80) { pureAscii = false; break; }
    }
    if (pureAscii) {
        fputs(buf, g_log);
        fputc('\n', g_log);
        fflush(g_log);
        return;
    }

    // UTF-8 → 系统 ANSI 代码页（GBK），避免日志中文乱码
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
    if (wideLen > 0) {
        std::wstring wide;
        wide.resize(wideLen - 1);
        MultiByteToWideChar(CP_UTF8, 0, buf, -1, &wide[0], wideLen);
        int ansiLen = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1,
                                          nullptr, 0, nullptr, nullptr);
        if (ansiLen > 0) {
            std::string ansi;
            ansi.resize(ansiLen - 1);
            WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1,
                                &ansi[0], ansiLen, nullptr, nullptr);
            fwrite(ansi.data(), 1, ansi.size(), g_log);
            fputc('\n', g_log);
            fflush(g_log);
            return;
        }
    }
    // 转换失败则原样写
    fputs(buf, g_log);
    fputc('\n', g_log);
    fflush(g_log);
}

extern "C" void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    LogV(fmt, ap);
    va_end(ap);
}

extern "C" void LogClose(void) {
    if (g_log) { fclose(g_log); g_log = nullptr; }
}

// 热键注册：写入 qol_hotkeys.txt（游戏根目录，UTF-8，追加模式）
// 格式：feature=key（每行一个），供 ModManager 自动读取覆盖
extern "C" void QolRegisterHotKey(const char* feature, const char* key) {
    if (!feature || !key) return;
    // 读取已有内容，若该 feature 已存在则先移除旧行
    const char* path = "qol_hotkeys.txt";
    char existing[4096] = {0};
    FILE* f = fopen(path, "r");
    if (f) {
        size_t n = fread(existing, 1, sizeof(existing) - 1, f);
        existing[n] = 0;
        fclose(f);
    }
    char featureLine[128] = {0};
    snprintf(featureLine, sizeof(featureLine), "%s=", feature);
    // 过滤掉同 feature 旧行
    std::string out;
    {
        std::string src(existing);
        size_t pos = 0;
        while (pos <= src.size()) {
            size_t eol = src.find('\n', pos);
            if (eol == std::string::npos) eol = src.size();
            std::string line = src.substr(pos, eol - pos);
            // 去掉行尾 \r
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.compare(0, strlen(featureLine), featureLine) != 0) {
                out += line;
                out += "\n";
            }
            if (eol == src.size()) break;
            pos = eol + 1;
        }
    }
    // 追加当前 feature=key
    out += featureLine;
    out += key;
    out += "\n";
    f = fopen(path, "w");
    if (f) {
        fwrite(out.data(), 1, out.size(), f);
        fclose(f);
    }
}