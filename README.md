# NeonCity

**一座海边的开放世界城市 · Unreal Engine 5 · C++**

![NeonCity](screenshots/city.jpg)

NeonCity 是我（丁浩城，物联网工程专业）在课余时间开发的第三人称开放世界游戏。
我负责玩法、系统架构和场景设计，AI 作为开发协作者帮我编写代码、排查问题；每个系统都配有自动化测试来验证。

## 数据一览

| 指标 | 数值 |
|---|---|
| C++ 源码 | 约 11.5 万行（其中自动化测试约 5.4 万行） |
| 源文件 | 约 990 个 |
| 自动化测试用例 | 500+ |
| 提交记录 | 148 次（2026.09.18 – 2026.10.27） |
| 引擎 | Unreal Engine 5.8 |

## 已实现的系统

| 模块 | 内容 |
|---|---|
| Player / Interaction | 玩家框架、数据组件、F 键交互、镜头扩展 |
| Vehicles / Aircraft | 汽车、越野车、船只、直升机驾驶 |
| Combat / VehicleCombat | 角色战斗、载具武器、爆炸与物理破坏 |
| AI / Police / Wanted | NPC 行为、警察与通缉系统 |
| Water / Ocean | FFT 海面、游泳、潜水、水体查询 |
| Adventure / Property | 海岛探险、宠物、车库、海边别墅等可交互建筑 |
| Economy / Phone | 经济与银行、游戏内手机 |
| Fishing / Leisure | 钓鱼、休闲设施 |
| Navigation / Missions | 地图导航、路线、任务与机会事件 |

## 玩家的家：海边玻璃别墅

![Villa](screenshots/villa_wide.jpg)

把出生点的旧房子推倒，从概念图开始重做：落地玻璃、无边泳池、屋顶露台。

![Villa detail](screenshots/villa.jpg)

## 仓库说明

- 本仓库公开的是 **C++ 源码、配置和开发文档**（`Source/`、`Config/`、`Docs/`）
- 美术资源（`Content/`）体积超过 15 GB，且包含第三方授权素材，因此没有上传
- 项目基于 UE5 第三人称模板搭建，`Variant_*` 目录来自模板
- 架构说明见 [Docs/Architecture.md](Docs/Architecture.md)

## 作者

丁浩城 · 物联网工程 · 目标：游戏客户端开发工程师
