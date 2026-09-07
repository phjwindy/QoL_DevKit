// feature.h —— Feature 抽象基类（崩溃隔离 + 状态管理）
#pragma once
#include <windows.h>
#include "logging.h"

namespace qol {

class Feature {
public:
    Feature(const char* name) : m_name(name), m_enabled(true), m_ready(false) {}
    virtual ~Feature() = default;

    const char* Name() const { return m_name; }
    bool  Enabled() const { return m_enabled; }
    bool  Ready()   const { return m_ready; }

    void SetEnabled(bool v) { m_enabled = v; Log("[%s] enabled=%d", m_name, (int)v); }

    // ★子类实现：扫 AOB + 装 Hook（只做一次）。失败 → m_ready=false + 日志 warn
    virtual bool Init() = 0;

    // ★子类实现：每帧逻辑（基座驱动或自建线程调用）
    virtual void Tick() = 0;

    // 默认 unload：子类可重写
    virtual void Unload() {}

    // 安全包装：Init 异常时捕获（不传播，避免拖垮其他插件）
    bool SafeInit() {
        Log("[%s] Init begin", m_name);
        bool ok = false;
        __try {
            ok = Init();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[%s] Init EXCEPTION — disabled", m_name);
            ok = false;
        }
        m_ready = ok;
        Log("[%s] Init %s", m_name, ok ? "OK" : "FAILED (disabled)");
        return ok;
    }

    void SafeTick() {
        if (!m_ready) return;
        // 被异常禁用后：每 kRetryIntervalTick 尝试恢复一次（重试成功即继续，失败则重新计时）
        if (!m_enabled) {
            if (++m_retryCounter < kRetryIntervalTick) return;
            m_retryCounter = 0;
            m_enabled = true;
            Log("[%s] Tick retry — re-enable", m_name);
        }
        __try { Tick(); }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            Log("[%s] Tick EXCEPTION — auto-disable", m_name);
            m_enabled = false; // ★自动禁用本功能，不影响其他；重试机制将在 kRetryIntervalTick 后再尝试
        }
    }

private:
    static constexpr int kRetryIntervalTick = 300; // 异常禁用后每 300 tick（约 5 秒@60fps）尝试恢复
    const char* m_name;
    bool        m_enabled;
    bool        m_ready;
    int         m_retryCounter = 0;
};

} // namespace qol
