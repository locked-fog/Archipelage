# Archipelago niri System Bar Design Guide

本文档基于 `tide-island-niri/` 的本地代码与 README 分析，定义新的 niri-first bar/shell：**Archipelago**。目标不是把 Tide Island 改成传统整条状态栏，而是把 Tide Island 的“一个主岛”拆成用户可排列、可组合、可独立展开的“多个岛”，同时保留 Tide Island 的核心视觉 DNA。

主要分析依据：

- `README.md` / `README.zh-CN.md`：产品定位、功能范围、配置项和依赖。
- `shell.qml`：每屏实例、IPC、全局快捷键、niri 下禁用 Hyprland overview 的行为。
- `DynamicIslandWindow.qml`：主胶囊形态、状态机、input mask、动画、loader 分层。
- `qml/island/*`：时间、歌词、自定义信息、通知、OSD、播放器等岛内视觉语言。
- `qml/controlcenter/*` / `qml/connectivity/*`：控制中心和连接详情的深色模块语言。
- `backend/StyleTokensBackend.*`：颜色、半径和动效 token。
- `backend/NiriBackend.*`、`backend/CompositorProvider.*`、`qml/common/CompositorDispatch.qml`：niri IPC 和 compositor 抽象边界。

## 1. 设计结论

Archipelago 应当是一个由多个动态胶囊组成的 system shell：

- 视觉上继承 Tide Island：黑色胶囊、强圆角、短而顺滑的 morph 动画、白色主文本、少量 iOS 风格系统色、内容被裁切在胶囊内部。
- 结构上脱离 Tide Island：不再由一个 `mainCapsule` 承担时间、歌词、自定义信息、通知、播放器、控制中心和 overview 的全部状态，而是由多个 island module 各自承载一个职责。
- 适配上优先 niri：工作区、窗口、输出、焦点状态都以 niri IPC 快照和事件流为第一数据源；Hyprland 支持可以作为后续 compositor adapter，而不是 MVP 的设计中心。
- 使用体验上不是“整条 bar”，而是“可编排的岛群”：用户能把工作区、媒体、系统状态、时钟、通知、连接状态放在左/中/右不同锚点，保留空白和轻量感。

## 2. Tide Island 的设计思路

Tide Island 的核心不是信息密度，而是“必要时出现”的状态反馈。

它的默认形态是一个极小的常驻胶囊，默认尺寸来自 `UserConfigBackend`：`islandWidth = 140`、`islandHeight = 38`、`islandPositionX = 50`。胶囊靠近屏幕顶部，`PanelWindow` 使用 top layer、少量 `exclusiveZone` 和精确 input mask，避免无关区域拦截桌面输入。

主视觉集中在 `DynamicIslandWindow.qml` 的 `mainCapsule`：

- `Rectangle` 使用纯黑填充，默认半径是高度的一半。
- 胶囊宽度、高度、圆角一起动画，形成从短胶囊到播放器、控制中心、通知、overview 的连续形变。
- 内容层通过 `Loader` 按状态挂载，所有内容被 `clip` 在同一个胶囊里。
- 动画大量使用 `Easing.OutQuint` / `Easing.InOutQuad`，常用时长在 120-400ms，主形变约 400ms。

交互上，Tide Island 把一个岛设计成多态对象：

- 左键/右键映射到可配置动作，默认分别是播放器和控制中心。
- 左右滑在时间、自定义信息和歌词之间切换。
- 音量、亮度、工作区、通知、蓝牙连接等临时状态会打断默认形态，短暂显示后自动恢复。
- 大面板仍从同一个胶囊展开，而不是弹出完全独立的窗口。

这套设计适合“一个主岛”，但当它承担系统栏职责时会遇到瓶颈：状态互斥过强、布局不可分组、多个系统信号争抢同一个显示位、niri 的工作区/窗口模型无法自然映射到 Hyprland 风格 overview。

## 3. 设计灵感

Tide Island 明确借鉴了 Dynamic Island：一个小型、黑色、可形变的顶部状态容器。它不是 macOS 菜单栏式的连续信息排布，而是把“正在发生的事”做成一个有生命感的状态对象。

从实现和命名看，Tide Island 的灵感还包括：

- 潮汐感：状态不是硬切换，而是从一个形态流入另一个形态。
- 小组件化桌面：歌词、媒体、通知、控制中心、自定义信息都不是完整应用，而是可短暂停留的桌面微界面。
- 安静实用的 Linux rice：默认不占空间，信息出现后快速退回。
- iOS 控制中心语言：深色模块、系统蓝/绿/黄/红状态色、圆角滑条、紧凑媒体卡片。

Archipelago 的灵感延展应当是“群岛”：

- Tide Island 是一个主岛，Archipelago 是多个小岛。
- 每个岛都保留 Tide Island 的形变语言，但只负责一个清晰职责。
- 多个岛之间有水域，也就是屏幕顶部的空白；不要把空白填成传统 bar 背板。
- 岛可以靠近、合并、隐藏、展开，但不应失去“独立漂浮”的识别度。

## 4. 视觉语言

### 4.1 形态

保留以下形态规则：

- 默认岛高度沿用 38px 量级，半径为高度的一半。
- 小岛可以有不同宽度，但都应是胶囊或大圆角矩形，不做尖角、标签页或硬边框。
- 展开态从原岛 morph 出来，优先改变宽度、高度、圆角和内部内容，不优先使用 detached popover。
- 大面板圆角可降低到 28-40px 区间，但必须仍像从胶囊长出来。
- overview 或大型切换器可以使用半透明深色卡片，但入口仍应由岛 morph 触发。

不要引入：

- 一整条实体背景 bar。
- 大面积渐变、发光球、装饰性玻璃纹理。
- 与 Tide Island 无关的品牌色主题。
- 卡片套卡片的视觉结构。

### 4.2 颜色

以 `StyleTokensBackend` 为基础建立 Archipelago tokens：

- Base：`#000000`、`#1c1c1e`、`#232326`、`#2c2c2e`。
- Text：`#f5f5f7`、`#8e8e93`、`#9b9da4`。
- Accent：`#0a84ff`、`#34c759`、`#ffcc00`、`#ff3b30`。
- Overview/large surface：保留接近 `#ee17181b` 的半透明深色和轻边框。

Archipelago 可以增加 token，但不能绕过 token 在组件里散落硬编码颜色。模块需要特殊色时，先判断是不是状态色；不是状态色就回到黑/灰/白体系。

### 4.3 字体和图标

继承 Tide Island 默认字体分工：

- 图标：`JetBrainsMono Nerd Font` 或用户配置的 Nerd Font。
- 正文/标题/时间：`Inter Display` 或用户配置字体。
- 数字和时间应稳、短、居中，不做大标题式夸张展示。

新组件应尽量避免继续新增负字距；若复刻 Tide Island 的旧文本层，需要先确认不会造成中文、CJK 混排或窄岛布局下的截断问题。

### 4.4 动效

Archipelago 的动效要保持 Tide Island 的“液态但克制”：

- 主 morph：320-420ms，优先 `OutQuint`。
- 内容淡入淡出：120-280ms。
- 数值变化：音量、亮度、电量、进度条使用 120-300ms。
- 临时反馈自动恢复：约 1.2s；通知 preview 默认 5s。
- 动画必须服务状态连续性，不为了装饰而持续运动。

## 5. niri-first 适配原则

Tide Island 已经有 niri 基础：

- `CompositorProvider` 通过 `$NIRI_SOCKET` 检测 niri。
- `NiriBackend` 连接 niri IPC socket，读取 workspaces、outputs、windows、focused window，并监听事件流。
- `CompositorDispatch.qml` 已经能把 workspace/window focus、move、close 路由到 niri。
- `CompositorWorkspaceTracker.qml` 能在 niri 下同步 focused workspace。
- 当前拖拽式 workspace overview 仍是 Hyprland-only，`shell.qml` 在 niri 下会跳过 overview toggle。

Archipelago 的 MVP 不应先移植 Hyprland overview，而应围绕 niri 的实际模型设计：

- 工作区岛优先展示 niri focused workspace index/name、output、当前窗口数量。
- 窗口岛优先展示 focused window 的 app id/title，并提供 close/focus/move 的动作入口。
- 多输出必须是一等布局输入：每个 `PanelWindow` 只管理当前 screen 的岛群，不用一个全局主岛假设所有屏幕共享焦点。
- workspace 切换优先支持相对切换、按 index/name 聚焦。
- 若 niri 无法提供可靠窗口预览，overview MVP 应是 workspace/window switcher，不渲染空的 Hyprland 风格网格。

## 6. Archipelago 信息架构

建议把 shell 分成以下层：

- `ArchipelagoShell`：Quickshell 入口，每屏创建一个 top layer shell。
- `IslandHost`：管理某个屏幕的岛群布局、输入区域、碰撞、overflow、exclusive zone。
- `IslandCapsule`：复用 Tide Island 核心胶囊视觉，负责形态、clip、边框、morph 动画。
- `IslandModule`：模块接口，提供 compact、expanded 视图；插件可另外通过 manifest 注册 preview template。
- `NiriCompositorService`：niri-first 数据层，输出不可变 snapshot，封装 IPC reconnect 和事件归一化。
- `SystemServices`：音量、亮度、电池、网络、蓝牙、通知、MPRIS、录屏等系统数据。
- `StyleTokens`：颜色、半径、间距、动效时长的唯一来源。
- `LayoutConfig`：用户配置的岛顺序、锚点、优先级、可见性和动作绑定。

关键约束：

- 一个岛只拥有自己的局部状态，跨岛协调交给 `IslandHost`。
- 同一时刻可以有多个 compact 岛，但 expanded 态默认只允许一个主展开面板。
- 临时反馈进入 `PreviewSurface`，从相关 compact 岛弹出；不要全局抢占所有岛。
- 数据层对 QML 暴露的是新 snapshot，不要求 QML 直接拼接底层 IPC JSON。

## 7. 默认岛群

当前 core 包默认只内置 `Time` 插件，作为第三方插件形态的参考实现。下面是面向完整插件生态的默认布局建议，`Workspaces`、`Media`、`System Cluster` 和 `Notifications` 应由第三方插件提供：

| 锚点 | 岛 | Compact | Expanded |
| --- | --- | --- | --- |
| left | Workspaces | 当前 workspace / 简短 strip | niri workspace + window switcher |
| center | Clock | 时间，可带录屏点 | 日期、日历、提醒占位 |
| center | Media | 播放状态或歌词短句 | 播放器 |
| right | System Cluster | 电池、音量、网络、蓝牙组合 | 控制中心 |
| right | Notifications | 常驻通知入口/未读数 | 通知详情/历史 |

默认不要启用太多岛。Archipelago 的第一印象应是安静、可扫描，而不是把 Waybar 的所有模块拆成圆角块。

## 8. 布局模型

布局配置应表达“锚点 + 顺序 + 优先级”。core 包当前示例配置只启用 `time`；安装第三方插件后可以把对应模块 id 加入锚点：

```json
{
  "anchors": {
    "left": ["workspaces"],
    "center": ["time"],
    "right": ["media", "system"]
  },
  "modules": {
    "workspaces": { "enabled": true, "priority": 90 },
    "time": { "enabled": true, "priority": 50 },
    "media": { "enabled": true, "priority": 60 },
    "system": { "enabled": true, "priority": 80 }
  }
}
```

布局行为：

- 岛间距默认 8-12px。
- 同一锚点的岛从内向外排列，避免中心岛被左右岛挤压。
- 宽度不足时按优先级折叠：先隐藏 passive text，再合并低优先级岛，最后隐藏最低优先级岛。
- critical 状态优先级最高，例如低电量、录屏、配对请求、网络密码请求。
- input mask 必须只覆盖岛和必要手势区域，不能让透明顶部区域吞掉应用点击。

## 9. 交互模型

每个 island module 至少支持：

- Primary click：打开或切换 expanded。
- Secondary click：打开模块菜单或执行用户配置动作。
- Scroll：只在语义明确时启用，例如音量、亮度、workspace。
- Hover：可选，默认不应自动展开大面板，避免误触。
- Swipe：保留 Tide Island 的横向“流动”手感，但只用于模块内部视图切换，不用于跨全局模式切换。

全局规则：

- expanded 面板打开时，其他岛保持 compact，但降低抢占性 preview。
- 通知不应覆盖用户正在操作的控制中心或播放器。
- 需要键盘输入的 Wi-Fi/蓝牙 prompt 才申请 keyboard focus。
- 所有 `qs ipc` 动作应有模块命名空间，例如 `archipelago workspace next`、`archipelago system toggle`。

## 10. niri 工作区和窗口体验

优先实现一个 niri-aware workspace island，而不是先做完整 overview。

Compact：

- 显示 focused workspace index 或 name。
- 可选显示当前 output 名称的短标识。
- 滚轮或快捷键切换 `r+1` / `r-1`。

Expanded：

- 左侧 workspace 列表，按 niri idx 排序。
- 右侧当前 workspace 的窗口列表，显示 app id/title。
- 支持 focus window、close window、move focused window to workspace。
- 无预览时使用清晰文本和图标，不渲染空白缩略图。

后续增强：

- 如果后续能稳定获得窗口几何和截图，再做视觉 overview。
- 若 niri 的输出/workspace 关系发生变化，adapter 负责归一化，UI 不直接依赖原始字段名。

## 11. 系统控制中心

可以复用 Tide Island 控制中心的模块语言，但要拆出职责：

- 音量/亮度滑条仍使用深色圆角模块和白色 knob。
- Wi-Fi、蓝牙详情面板可以继续作为 side detail shell，但入口应来自 `system` island。
- 电池/TLP 功能需要重新审视安全边界：不要默认鼓励在配置里保存 sudo 密码；优先使用 polkit/明确提权提示，且永不记录 secret。
- 所有系统命令必须有超时、错误消息和不可用状态。

控制中心不是 Archipelago 的唯一主面板。媒体、工作区、通知都可以有自己的 expanded 面板，但同一时刻默认只有一个大面板打开。

## 12. 配置策略

首版可以延续 Tide Island 的 JSON 配置模式，因为现有 `UserConfigBackend` 已经支持：

- 注释剥离。
- 文件 watcher 热重载。
- 默认值合并。
- 鼠标按键和动作映射。

Archipelago 配置建议使用新路径，避免污染 Tide Island：

```text
~/.config/archipelago/config.json
```

需要从一开始支持：

- 模块启用/禁用。
- 锚点、顺序、优先级。
- 每个模块 compact 展示方式。
- 鼠标动作和快捷命令。
- niri bind snippet 生成或检查。
- 字体和 style token 覆盖。

配置加载必须校验类型和范围。无效配置应回退默认值并展示可读错误，不应让 shell 崩溃。

## 13. 验收标准

Archipelago MVP 完成时应满足：

- 在 niri 下不依赖 Hyprland 环境变量即可启动。
- niri socket 断开后能重连，UI 显示降级状态。
- 多显示器下每屏岛群位置正确，不互相抢焦点。
- 透明区域不拦截应用输入。
- 默认布局在 1366px 宽度仍不重叠。
- 工作区切换、窗口聚焦、窗口关闭有明确失败反馈。
- 正常 idle CPU 接近 Tide Island 的目标水平，避免持续高频动画。
- 新增 C++ 后端有单元测试；配置解析和 niri event parsing 必须覆盖错误输入。
- 不新增硬编码 secret；提权功能默认安全关闭或显式提示。

## 14. 不应照搬的 Tide Island 部分

- 不要继续让一个 `mainCapsule` 管理所有状态。
- 不要把 Hyprland-only overview 当作 niri MVP。
- 不要把 `islandPositionX` 这类单点定位扩展成所有模块的布局模型。
- 不要让 passive preview 无条件覆盖用户正在操作的面板。
- 不要把系统控制中心作为所有系统状态的唯一入口。
- 不要默认保存 sudo 密码。

## 15. 应保留的 Tide Island 部分

- 黑色胶囊和大圆角。
- morph 而不是硬弹窗。
- clipped content + lazy loader 的组件挂载方式。
- 短时、顺滑、跟手的动画。
- 精确 input mask。
- 系统反馈自动恢复。
- MPRIS、歌词、通知、Wi-Fi、蓝牙、电池、亮度、音量这些数据能力。
- 用户可配置点击动作和字体。

## 16. 命名约定

- 产品名：`Archipelago`。
- 单个可视模块：`Island`。
- 胶囊视觉基元：`IslandCapsule`。
- 模块接口：`IslandModule`。
- 一屏的岛群管理器：`IslandHost`。
- niri 数据层：`NiriCompositorService`。
- 大面板：`ExpandedSurface`，避免叫 popover，强调它从岛生长出来。
- 预览面板：`PreviewSurface`，用于通知、连接详情、配对确认等 transient/secondary UI。

## 17. 实施提醒

Archipelago 的核心判断标准是：看起来仍像 Tide Island 的亲缘设计，但使用方式已经是 niri 的 system bar。

当出现设计冲突时优先级如下：

1. niri 的实际工作流。
2. Tide Island 的核心视觉 DNA。
3. 用户可自定义排列。
4. Hyprland 兼容。
5. 装饰性视觉增强。
