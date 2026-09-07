// logging.h —— 统一日志：qol_<feature>.log（游戏根目录，追加写，无缓冲）
// API 兼容两套约定：
//   - 旧骨架：LogOpen(name) / Log(fmt,...) / LogClose()
//   - 新骨架：qol::SetLogName(name) / qol::Log(fmt,...)
#pragma once
#include <cstdio>
#include <cstdarg>

#ifdef __cplusplus
extern "C" {
#endif

// 打开日志文件（同一进程多次调用会切换文件名并重开；请在加载线程调用一次）
void LogOpen(const char* feature);
// 可变参数版本
void Log(const char* fmt, ...);
// va_list 版本（供命名空间转发）
void LogV(const char* fmt, va_list ap);
// 关闭日志文件
void LogClose(void);
// 注册 MOD 热键：写入 qol_hotkeys.txt（供 ModManager 自动读取）
//   feature: MOD 英文名（小写，如 "autofish"）
//   key:     热键描述，支持多键（如 "9"、"4, D-pad Up"、"F6"）
//           多键用逗号分隔，ModManager 会全部显示并逐个检测冲突
// 注意：不以 RegisterHotKey 命名，避免与 Win32 API 重名冲突
void QolRegisterHotKey(const char* feature, const char* key);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace qol {

// 与全局 C 函数等价（提供命名空间风格，供新代码使用）
inline void SetLogName(const char* feature) { ::LogOpen(feature); }

inline void Log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    ::LogV(fmt, ap);
    va_end(ap);
}

struct ScopeLog {
    const char* name;
    ScopeLog(const char* n) : name(n) { ::Log("[%s] enter", name); }
    ~ScopeLog() { ::Log("[%s] leave", name); }
};

} // namespace qol
#endif