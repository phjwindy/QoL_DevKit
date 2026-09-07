---
AIGC:
  ContentProducer: '001191110102MAD55U9H0F10002'
  ContentPropagator: '001191110102MAD55U9H0F10002'
  Label: '1'
  ProduceID: '24828ff7-9e32-4c59-a40b-753dbb3a63fc'
  PropagateID: '24828ff7-9e32-4c59-a40b-753dbb3a63fc'
  ReservedCode1: '54bf6817-01af-463c-9be1-f8aca7a898b6'
  ReservedCode2: '54bf6817-01af-463c-9be1-f8aca7a898b6'
---

# QoL DevKit — Village in the Shade MOD 开发套件

> 作者：PHJ&消失的清风  
> 转载或分享时请注明出处。

## 项目简介

《Village in the Shade》（静谧田园）QoL（Quality of Life）MOD 合集，基于 C++ DLL 插件架构，通过 AOB 签名定位 + 函数 Hook 实现游戏体验优化。当前适配游戏版本 **v1.09 (build 25094764)**。

## 目录结构

```
QoL_DevKit_Git/
├── QoL_Shared/          共享框架（6 个模块，所有 MOD 复用）
├── PluginTemplate/      新 MOD 脚手架模板
├── AutoFish/            自动钓鱼
├── AutoHarvest/         树液自动收集
├── ChestSort/           箱子快速归类
├── EyeFix/              结局后保持正常眼
├── ModManager/          游戏内 MOD 开关管理器
├── MonsterMark/         夜间怪物/宝箱地图标记
├── ProductionAuto/      生产链自动化（投料/收料/绑定）
├── Scarecrow/           稻草人与洒水器重叠
├── SickleHarvest/       镰刀范围收割
└── Sower/               范围播种
```

## 共享框架（QoL_Shared）

所有 MOD 共用的基础设施，编译时通过 `#include` 引入：

| 模块 | 功能 |
|------|------|
| `logging.h/.cpp` | 文件日志（`qol_<feature>.log`），FileShare 兼容读锁 |
| `aobscan.h/.cpp` | AOB 签名扫描，定位游戏函数 RVA |
| `hook.h/.cpp` | Trampoline Hook 安装/卸载，支持热移除 |
| `hotkey.h/.cpp` | 统一热键管理，支持键盘+手柄，运行时改键 |
| `feature.h` | MOD 基础接口定义（mod_init/mod_tick/mod_unload） |
| `safe_call.h` | SEH 安全的原生函数调用包装（解决 C2712） |

### safe_call.h 使用要点

游戏原生函数调用时，`__try/__except`（SEH）不能和带析构的 C++ 对象在同一函数块使用（MSVC C2712）。`safe_call.h` 提供 NOINLINE 纯 POD wrapper 函数隔离 SEH：

```cpp
#include "safe_call.h"

// bool 返回，1 参数
bool result = qol::SafeCallBool1(g_someFunc, status);

// int 返回，1 参数（SEH 异常时返回哨兵值 -1）
int id = qol::SafeCallInt1(g_someFunc, status);

// bool 返回，2 参数，带状态码输出
int code = -1;
bool ok = qol::SafeCallOutBool1(g_someFunc, status, &code);
```

## 各 MOD 功能与状态

| MOD | 功能 | 当前版本 | 状态 |
|-----|------|----------|------|
| AutoFish | 钓鱼小游戏自动完成+循环抛竿+垃圾排除 | v1.4.0 | 已验证 |
| AutoHarvest | F4 收集范围内树液提取器树液 | v1.8.4 | 已验证 |
| ChestSort | 数字键 4 触发箱子内物品自动归类 | v1.3.4 | 已验证 |
| EyeFix | 数字键 0 切换：结局眼↔正常眼 | v1.1.2 | 已验证 |
| ModManager | F2 面板管理 MOD 开关，手柄 L1+R1 | v1.1.4 | 已验证 |
| MonsterMark | 夜间自动标记鬼鼠/宝箱/书籍/齿轮 | v1.0.28 | 已验证 |
| ProductionAuto | F5 切 HUD，自动投料/收料/设备绑定 | v1.1.25 | 已验证 |
| Scarecrow | 稻草人与洒水器可放置在同一格 | v1.0.3 | 已验证 |
| SickleHarvest | 挥镰刀范围收割成熟作物+果树摇树+木耳 | v2.2.4 | 已验证 |
| Sower | 数字键 5 切换 1x1/3x3/5x5/连通播种 | v1.3.3 | 已验证 |

## 开发环境

- **编译器**：MSBuild via vswhere，x64 Release，vcxproj PlatformToolset v145 + `/utf-8`
- **源码编码**：UTF-8 无 BOM，行尾 CRLF
- **加载方式**：`steam_api64` 桥接加载器，扫描 `mods\` 目录下 DLL
- **MOD 目录格式**：`mods\<MOD名>_v<版本号>\<MOD名>.dll`
- **DLL 接口**：每个 MOD 导出 `mod_init` / `mod_tick` / `mod_unload` 三个函数

## 游戏更新适配流程

游戏发布新版本后，按以下步骤适配：

1. **计算新 exe 指纹**：SHA-256 + 文件大小
2. **AOB 重新扫描**：用 `aobscan2.py` 对新 exe 执行签名扫描，定位所有函数 RVA
3. **RTTI 重定位**：用 `rtti_scan2.py` 重新定位 vtable 类型地址
4. **逐个 RVA 验证**：用 `verify_rvas.py` 实读字节验证每个 RVA 处的函数序言
5. **配方表更新**：用 `recipe_scan2.py` 重新提取配方表
6. **VerifyExeBuild 硬编码 SHA 更新**：各 MOD 的 `version_manifest.h` 中 exe SHA 需更新
7. **编译部署+实测**：先适配 2~3 个核心 MOD 实测通过，再批量适配剩余

### 关键注意

- **代码非整体平移**：各函数偏移不等，.rdata/.data 段不能用 delta 推算
- **AOB 三层验证法**：①字节签名全 .text 搜索 → ②唯一匹配验 prologue → ③RVA 实读字节验证
- **四大坑**：vtable 重定位、VerifyExeBuild 硬编码 SHA 过期、E8 call rel32 位移跨版本变化、vtable 槽位函数整体重定位

## 已验证可用的原生函数

| 函数 | RVA | 用途 |
|------|-----|------|
| `output_helper` | 0x165DF0 | 物品物化+转移核心 |
| `native_load` | 0x26FF20 | 生产机器投料 |
| `StatusSearchByBaseID` | 0x250690 | 按 baseID 查找 GimmickStatus |
| `spatialSearch` | 0x189990 | 空间索引搜索 |
| `map_find` | 0x150B10 | 哈希表查找 |
| `HarvestSettle` | 0x211DD0 | 作物收割结算（SickleHarvest 木耳关键） |

## 开发原则

1. **原生函数优先**：涉及游戏系统交互时，优先调用原生函数（AOB 定位→直接调用），而非自写逆向逻辑。原生函数天然兼容所有槽位数和容器类型，自写逻辑天然脆弱
2. **死档红线**：禁止手写清槽/重置状态，必须走原生 event notify 链
3. **诊断优先于改码**：功能异常先读日志定位根因，不臆测
4. **发布版干净**：发布版关闭所有诊断日志，不残留调试代码
5. **备份先行**：修改源码前必须备份原文件

## 后续开发线索

### SickleHarvest 木耳收割（已实现 v2.2.0+）

完整诊断历程（v1.2.1~v2.2.0 共 10 轮诊断）已沉淀，核心结论：
- 木耳不在 spatialSearch 空间索引中，不走命令处理器/Retrieve 状态机
- 原生镰刀接受木耳目标但直接放行不执行 HarvestSettle
- 最终方案：MOD 侧对原生接受的目标执行 HarvestSettle 收割，但**不写回目标列表**（避免原生重复处理导致双倍产出）
- 调用链：HarvestSettle(0x211DD0) → ShakeTreeSettle(0x19D320) → item_ctor(0xFF870)，itemId=0x187E0

### 镰刀范围收割长期目标

覆盖三类：**采集物、作物、花**。当前作物+果树+木耳已完成，花类尚未覆盖。

### MonsterMark 山顶标记

山顶区域 2 个锚点（零件/书）在原生锚点查询 (0x1C1A00) 中返回 found=0，用户接受现状暂过关。后续可尝试：
- 跟踪原生锚点查询的 areaId 参数，确认山顶区域 ID 是否被正确传入
- 或换用 spatialSearch 补充扫描山顶锚点

### ProductionAuto 链式自动化

已有投料/收料/设备绑定/地板互联功能。潜在优化方向：
- 蚕盒链路产出转移的稳定性验证
- HUD 绑定数漂移的长期监控

### EyeFix

源码中缺少版本号标识（其他 MOD 在 .cpp 头部注释中有 `v1.x.x`），建议补充版本注释便于追踪。

## 插件开发快速上手

1. 复制 `PluginTemplate/` 目录，重命名为新 MOD 名
2. 修改 `plugin.cpp` 中 3 处标记（功能名/AOB 签名/Tick 逻辑）
3. 修改 `.vcxproj` 中的项目名和输出 DLL 名
4. 在 `version_manifest.h` 中填入游戏的 exe SHA-256 和 build 号
5. 编译产出 DLL，放入 `mods\<MOD名>_v<版本号>\` 目录

### Hook 模式

```cpp
// 安装 hook（mod_init 中）
g_origFunc = (FuncType)InstallHook(targetRVA, detourFunc, hookSize);

// detour 函数中调用原函数
auto result = g_origFunc(args...);

// 卸载 hook（mod_unload 中）
RemoveHook(g_hookHandle);
```

### 日志模式

```cpp
LogOpen("myfeature");  // mod_init 中，创建 qol_myfeature.log
Log("info: value=%d", val);
// 发布版通过注释 #define 关闭日志
```

---

> 本项目为个人兴趣开发的免费 MOD，不涉及任何商业用途。  
> 游戏版权归原开发者所有，MOD 仅改善玩家体验，不修改游戏核心数据。  
> 作者：PHJ&消失的清风，转载或分享时请注明出处。

> AI生成