# Maniac 缺口补全执行计划

制定日期：2026-09-25。状态：计划完成，实施尚未开始。本文接续 [兼容性评估](maniac-250921-compatibility-assessment.md)；[168 项入口清单](maniac-250921-command-inventory.csv) 是静态盘点，不是功能通过清单。本文附录为全部编号指定主责工作包。

## 1. 目标、边界和完成定义

目标：将当前零散 Maniac 支持补全为可追溯、可回归的运行兼容实现，覆盖新增命令、基础命令的扩展参数、共享运行机制、存档和平台后端。按固定原版行为实现，包括游戏可能依赖的确定性差异；不根据命令名称或编辑器预览猜测行为。

规划采用以下基线，实施前将实际文件、配置和哈希登记到验收材料：

|维度|计划基线与交付要求|
|---|---|
|主参考版本|`patch_selfvar_250921`；168 个目录编号及其实际运行子模式|
|旧版本|241028 单列差异验证；不能用新版结构完成结论代替旧版语义验证|
|平台顺序|Windows 原版对照先行；Web、Android 分别完成可用能力及差异验收|
|持久化|Player 保存/恢复完整运行状态是必需项；原版 Maniac 存档双向互通设独立工作包和完成状态|
|回归基线|原生 RPG Maker 2000/2003、现有 EasyRPG 扩展及本地已实现的 Maniac 修复|
|范围外|编辑器功能、RPGRewriter 产品改写、任意第三方原生插件、未知未来版本、站点业务重构|

“Windows 250921 完整兼容”须满足：范围内每条命令的每个有效子模式已实现或证明与原版空操作一致；共享状态、时序和存档恢复验证完成；没有未关闭的必需功能缺口。原版存档互通未完成时只能声明“运行兼容，原版存档互通未完成”，不能概括为无条件完整兼容。

Web/Android 对窗口、系统鼠标、文件路径等能力如存在无法等价的项目，发布声明必须列明限制；不得将降级、静默跳过或告警占位计为完整。所有版本/平台的完成度分别记录，不用一个总百分比掩盖差异。

本次只制定计划和责任分配，不实施运行代码、新增测试、游戏/UI/浏览器操作或发布。后续涉及新增测试代码、人工 UI/浏览器交互和子代理委托时，遵守本会话提供的 AGENTS.md 授权约束；本计划不视为这些操作已获授权。

## 2. 已核对的当前基线

- Player HEAD：`bec5c46f2043334329a9b6c4dcf4ed660c81d553`。
- liblcf HEAD：`4056dbf533182dd46c0a9b1831da23bc16cc0ae8`，检查时工作区干净，是独立仓库。
- 报告：`../RPGRewriter-maniac/analysis/Maniacs-Complete/reviewed-contracts.json`，168 个编号，SHA-256 `168ee47d0918308c00efa77bc63cfad7efc34a30e25c23db6579d8f61f4f5b57`，与 9 月 22 日一致。
- 重新校验旧评估中的七个代码文件哈希全部一致：三个事件解释器、共享解释器 `.cpp/.h`、`maniac_patch.cpp`、liblcf 指令枚举。因此旧入口盘点仍可用；不代表全仓库无变化。
- 当前未提交工作主要是 Web 音频、资源读取、视频后端及构建/打包。见 [web-audio.md](web-audio.md)、[web-movies.md](web-movies.md)。这些是工作区实现文档，本轮没有重新验收运行效果。
- 旧评估提到的长图、动画和循环修复必须保留。历史记录中解释器测试曾在进入断言前报 `vector too long`，本轮未复现；W00 必须重新判断当前基线，不能把历史失败当作当前失败，也不能跳过。

当前 3001—3038 中，3001—3021 有入口但不全；3025/26/28/29、3030/32/33/36/37/38 缺少相应分派；3027、3031 应由父命令消费；3022/23/24/34/35 原版独立执行无载荷效果。基础命令的扩展参数同样在补全范围，不能只补这些新编号。

## 3. 追踪方式：每个缺口必须能关闭

W00 建立逐子模式台账，最小字段如下：

`ID / opcode / variant / 原版版本与哈希 / 场景上下文 / 平台 / 参数与模式 / 短记录规则 / Text 角色与长度单位 / 读写对象 / 生命周期与等待 / 存档字段 / Player 实现位置 / 证据位置 / 缺口类别 / 依赖 / 验收例 / 当前状态 / 实际结果 / 阻断原因`。

缺口类别：缺失、部分实现、行为不同、待核实、原版空操作、父命令消费、平台受限。状态：待审计 → 契约就绪 → 实现中 → 待验证 → 验证通过；阻塞和平台限制独立标识。没有专项结果的旧代码只能标为“待核实”，不能默认通过。

台账不机械展开无效模式笛卡尔积。以报告中的独立行为分支、局部模式表、读写上下文和平台差异为条目；数值边界通过验收用例覆盖。父命令与续行、生产状态的命令与消费状态的命令相互链接。

证据优先使用已审核运行时消费者和实际原版对照。writer 用于确定合法输出及裁剪规则，formatter 用于理解显示结构；与 runtime 冲突时保留差异，不统一为猜测的“正确格式”。缺少运行机制证据时登记定向补证问题，给出具体函数、消费者和阻断工作包。

## 4. 实现约束与关键设计决策

1. 复用当前解释器、地图、图片和战斗系统。共用逻辑只按已证实相同的契约抽取；不建设通用指令框架，不把分析 JSON 变成游戏运行依赖。
2. 将 Maniac bank/context 接入普通数值、字符串和开关访问路径；不要仅在 3036 内模拟自变量。明确全局、公共事件、地图事件、当前帧和调用者环境的所有权、分配时点及生命周期。
3. `CmdSetup` 已会补零，保留这项能力；对依赖原参数数量、尾部字节码或短记录的命令使用真实长度。不得以编辑器创建默认值补读旧数据。缺失字段与未知模式分别处理。
4. 数值运算明确 int32 位模式、移位、溢出和除零策略；不要依赖 C++ 未定义行为。按原版证据处理确定性行为，对原版越界、未初始化数据和崩溃路径记录安全偏差，不复制内存破坏。
5. 区分工程编码字节、UTF-16 单元、UTF-8 字节和原版近似宽度算法。不同命令确有差异时保留各自规则；字符串索引、正则、文本长度不能统一为 Unicode 字符数。
6. 版本语义与功能开关分离。W00 确认现有 Maniac 识别能力；无法可靠识别 241028/250921 的地方不猜版本，必要时提供明确的版本选择。旧游戏默认行为改变必须有依据和回归证据。
7. liblcf 枚举/存档修改从 `generator/csv`、模板及相关序列化源头进行，再生成文件；不只手改 generated 头文件。持久状态随对应功能落地，不拖到最后补序列化。
8. 新状态若无法写入原版格式，不静默丢弃。Player 扩展保存与原版保存协议分开；是否可双向互通必须由字段和恢复语义证明。
9. JS 引擎先确认原版引擎版本、初始化、宿主绑定和所需语义，再选择可维护的嵌入方式；不预先指定依赖。Web 版不能简单用页面 `eval` 替代，否则上下文和同步行为会偏离原版。
10. 平台行为经过现有后端接口。特别是 Web 音频异步快照、视频完成回调和持久化确认，要明确命令等待、过期消息和错误反馈；不为 Maniac 另造一套播放系统。

## 5. 工作包、依赖和验收

下列工作包均要求交付代码、更新后的台账和验证材料。新增测试及 UI 验收在取得相应授权后执行；未执行时保留“待验证”。复杂度表示相对风险，不是工期承诺。

### W00 — 基线、证据缺口与验证设施（先行，高）

- 冻结 Player/liblcf/报告版本、构建工具链、参考 RPG_RT 文件与哈希；记录当前脏文件，保留 Web 工作。
- 从附录 168 个编号继续拆解逐子模式台账，登记共享 helper 的所有调用方；建立版本、平台和场景分区。
- 重新运行相关既有非 UI 检查，定位解释器历史启动失败是否仍存在。新增测试设施获授权后，以现有 doctest/mock_game 为入口，不建立第二套测试框架。
- 设计原版对照夹具：相同项目/资源、输入序列、随机条件、初始状态、帧编号；记录变量、开关、字符串、调用栈、图像、音频事件、文件和保存恢复结果。原版不可直接观察的状态用最小事件工程导出，观察手段不得改变待测命令语义。
- 先执行 JS 初始化/宿主 API、原版存档、自变量生命周期、241028 差异四项定向补证，尽早暴露后续阻断。
- **完成条件**：168 个编号有主责，所有运行子模式有处置/明确补证项；参考版本可复现；既有检查结果真实登记；可执行的首批验收样例定义就绪。

### W01 — 参数求值、环境与自变量（依赖 W00，高）

- 实现 3036 的 Set/Clear/Init、五类环境、数字/字符串/开关 bank、标量与数组、名字解析、负编号、间接引用及首次分配规则。
- 统一需要同一行为的访问入口；将 interpreter 上下文实际传入 `game_interpreter_shared`、变量/开关/字符串和相关消费者。保留各命令局部模式差异。
- 明确公共事件持久 bank、按地图/事件分组 bank、帧绑定和调用者查找；地图切换、返回标题、新游戏、读档分别处置。
- 修改面：`game_interpreter_shared.*`、`game_interpreter.*`、`game_variables.*`、`game_switches.*`、`game_strings.*`、地图/公共事件和 liblcf 必需状态。按实际生命周期决定是否增加小型共享状态类。
- **验收**：嵌套/递归/并行事件隔离；当前/调用者读写；负引用传播；Clear/Init 不意外创建环境；不同地图同事件编号；同一存储区域上的复制/覆盖；保存恢复后身份和数值一致。

### W02 — 调用、返回、分支与调度（依赖 W01，高）

- 12330 的全部 selector、名字/偏移、带类型参数、接收地址；12310 的转换、返回写入和清理顺序；1005 及帧结束关系。
- 标签、嵌套循环/外层 break/continue、条件分支、事件擦除恢复、等待和父子续行；核对 0/10/40、3022/23/24/34/35 的取指与 yield，不虚构功能。
- 已有循环修复逐项回归；调用参数按原版顺序写入，别名和间接地址可能受前一步写入影响。普通帧结束不自动等同显式返回。
- 修改面：三个事件解释器及共享状态、地图事件更新和 liblcf 执行帧字段。
- **验收**：正常/条件/循环嵌套返回、并行事件、忙碌重试、续行游标、无意外多等/少等一帧；等待和嵌套调用中保存恢复；非 Maniac 控制流保持原行为。

### W03 — 变量数组与表达式（依赖 W01/W02，高）

- 10210/10220、3013/3019；补齐报告中的节点、函数、array/range/subscript、self/address/dot、赋值与副作用、广播/复制/别名。
- 字节码使用解码后的 int32 按小端还原，不截断为单字节；处理真实长度、截断、节点边界及调用者转换差异。
- 动态命令调用继承正确上下文和求值时点；保持 EasyRPG 扩展已有策略，与 Maniac 模式分别验收。
- 修改面：`maniac_patch.*`、`game_interpreter_control_variables.*`、共享解释器、变量/开关操作。
- **验收**：每个已定义节点和内置函数至少有行为例；短路、写入顺序、数组别名、正负边界、逻辑右移和格式错误有明确结果；不能仅以解析成功验收。

### W04 — 字符串、编码、正则和文件（依赖 W01/W03，高）

- 3020 全部操作及来源：赋值/拼接、数字/数据库文本、插入替换截取、Join/Split/PopLine、搜索捕获、格式化及文件读写。
- 逐分支核对来源游标、长度单位、CR/LF、内嵌 NUL、空串、范围别名、正则异常、替换语法和失败后的写入结果。
- 原版 Insert 预览/执行顺序不同、文件编码路径不同等已知差异保留在契约中；不能按 UI 标签修正原版执行行为。
- 修改面：`game_strings.*`、字符串命令 handler、编码及文件访问辅助设施；由 W10 接平台路径/持久化策略。
- **验收**：工程编码样例、日文/中文/半角/代理项、零长度和异常正则、重叠范围、文件失败、换行边界；Windows 与 Web/Android 分别验证文件结果。

### W05 — 全部基础命令的扩展参数（依赖 W01/W02，跨度大）

- 对附录 W05 全量复核：角色/物品/队伍、资源名、传送/坐标、地图修改、菜单/商店/旅馆、输入姓名、计时及转场。
- 重点处理变量化/字符串资源名、负引用、短记录、未知位、状态刷新和事件页重新计算；不能因普通 RM 命令已支持就跳过 Maniac 扩展。
- 3015 RewriteMap、3021 GetGameInfo 与实际地图/图片/解释器消费者联动；像素和解释器信息依赖 W02/W06 的最终状态。保留 10860 的既有精确兼容行为。
- 移动路线由本包主责：11330 与连续 3027 的收集、变量捕获时点、内层字节流、动作编译、等待/停止、重复/可跳过规则和 11310/11320；涉及图片/声音表现分别与 W06/W08 联验。路线可以先完成数据与执行语义，再在表现里程碑收口。
- 修改面：三个解释器、角色/队伍/地图/场景实现及命令实际触及的 liblcf 数据。
- **验收**：每个附录编号的全部有效扩展模式闭合；切图、角色/队伍变化、事件刷新、菜单返回和结果分支可观察结果正确。

### W06 — 图片、文字、消息和屏幕（依赖 W02/W04，高）

- 11110/20/30、3007/08、3017、3025/26/28、3032：图片状态、ID 变更、文字图、像素编辑/读出、图块绘制、输出、缩放、动画帧及持续效果。
- 10110/20/30/40/50、20110/40/41、3029：消息框尺寸/字体/间距、控制码、选择分支、输入窗口、消息钩子和重入顺序。
- 11010—11070、11210：屏幕和动画，核对持续时间、等待、对象空间、层次和存档；保留现有动画缓冲和长图路径。
- 修改面：`game_pictures.*`、`sprite_picture.*`、`game_screen.*`、消息/窗口/字体/bitmap 与相关 handlers；原版规则与渲染后端限制分开。
- **验收**：固定资源/字体下的像素或事先定义容差的图像对照、图层遮挡、变换组合、钩子顺序、图片改号后的引用、动态图片保存恢复。字体/抗锯齿存在差异必须列项，不能用宽泛容差掩盖布局错误。
- 图片输出复用编码设施，区分截图时间点、裁剪区域、透明度和写盘完成；Web 导出由 W10 适配。

### W07 — 战斗与战斗钩子（依赖 W01/W02/W03，中高）

- 3009—3012、3037、1006—1009、10710 和分支、13110—13410 目录内全部战斗命令、5003。
- ATB 更新/等待模式、行动次数、敌群成员增减、目标与索引、战斗信息、指令替换、强制逃跑和终止；3018 内战斗钩子与 W08 联动。
- 修改面：`game_interpreter_battle.*`、战斗场景/算法、角色敌人状态和对应存档约束。
- **验收**：相同初态/随机条件的行动顺序、ATB、状态改变、胜负逃跑分支、事件嵌套和等待；运行中变更队伍不留下悬空目标。只验数值、不验行动顺序不算完成。

### W08 — 游戏选项、音频和视频（依赖 W02；钩子依赖 W06/W07，高）

- 3018 全部实际选项/钩子、3038、11510—11560、系统 BGM/SE 资源关联。逐项核对帧率/游戏时间、位置/循环状态、音量/音调/淡入淡出和电影等待。
- 明确游戏逻辑帧、渲染帧、音频采样时钟及暂停的关系；不得通过修改一个全局 FPS 常量冒充原版变速语义。
- 复用当前 Web 音频和视频工作：事件序号、generation/session、异步状态快照、播放结束/失败/暂停，以及过期回复丢弃。接入前读取相关任务最终状态和代码，不能回退成旧后端。
- 修改面：音频接口/后端、电影播放器、时间步进、相关事件 handlers、Web Worker 消息接口。
- **验收**：连续播放/切换/淡出/重播、帧率切换、暂停恢复、自动播放拒绝、视频结束/错误、过期回复及停止清理；记录允许的快照延迟和原版差异。跨设备性能单独实测。

### W09 — JavaScript 和宿主接口（补证从 W00 开始；实现依赖 W01/W02/W04，高风险）

- 补齐初始化、全局对象、绑定函数/属性、版本、功能开关、上下文生命周期、异常和引擎控制流程的证据；现有 3030 字段契约不足以证明这些全部完整。
- 3030 字面量/字符串来源、连续 3031、共享上下文、结果数组/标量、逐目的地转换和写入；核对 getter 等副作用顺序及普通异常与引擎控制异常的区别。
- 引擎选择形成短设计决策：语义版本、宿主 API、Windows/Web/Android 构建、代码体积、维护及许可材料。不能只接一个 JS 求值入口就关闭工作包。
- **验收**：上下文跨命令保留、续行/引用来源、负目的地、数组属性和转换副作用、异常/退出/重新开始、嵌套宿主调用、资源释放；存档是否保存/重建 JS 状态按原版证据处置。
- **阻断规则**：原版宿主绑定仍不明确时，明确阻塞相关台账行；可继续独立工作包，不能把普通 JS 标准测试当作宿主兼容证明。

### W10 — 输入、控制台和平台能力（依赖相应 W04/W06/W08/W09，中高）

- 3005/06/14、11610、3033、5004/05 和关联窗口选项；鼠标坐标变换、定位、键盘/手柄状态、按下/持续/释放、焦点及控制台文本展开。
- Windows 对照桌面行为；Web 明确游戏内虚拟鼠标与系统光标、下载与文件保存、全屏手势、页面焦点等差异；Android 明确触摸/硬件键盘/手柄、应用存储和窗口限制。
- 每个平台返回值、错误反馈、阻塞/异步策略明确；确实不可等价的条目保留限制，不伪装成功。
- **验收**：实际输入设备、DPI/缩放/黑边坐标、失焦恢复、输出文件/图片、权限拒绝和重试。Web 浏览器、Android 真机/模拟器环境及能力单独记录。

### W11 — 存档、全局保存和原版互通（格式调查从 W00 开始；随 W01 起持续落地，高风险）

- 3001—3004、3016；所有其他工作包产生的持久状态登记到同一字段表：所有者、默认值、版本、保存时点、恢复顺序、旧档回退、原版对应字段。
- Player 保存恢复先闭合：self bank、执行帧/返回目的地、图片像素及效果、消息状态、地图修改、可保存的等待/钩子等，依据原版实际保存语义决定持久与瞬态。
- 原版互通分别验证 原版→Player、Player→原版、Player→Player；记录原版无法表达的 Player 扩展。不能用只加载成功替代恢复后继续执行正确。
- 原版文件必须备份后在副本中验证，保留原始 hash；错误、写入中断、损坏档、旧档、共享档和新游戏隔离均有明确定义。
- 修改面：liblcf generator/reader/writer、Player 保存/恢复、执行帧及各状态所有者。
- **验收**：在嵌套调用/等待/切图/效果进行中等有代表性检查点保存，重启恢复后比较后续轨迹；共享保存不串游戏；Web 写入确认失败不得报告保存成功。

### W12 — 241028 差异、跨平台回归与发布收口（依赖所有必需工作包，高）

- 241028 的 formatter 局部变化筛选只能定位调查对象；按实际消费者补齐旧版证据，确定哪些行为共享、哪些需版本分支，不复制两套解释器。
- Windows/Web/Android 分别运行已授权的功能验证；原生 2000/2003、EasyRPG 扩展、Maniac 旧游戏和新 selfvar 工程分别回归。
- 完整打包 Player、liblcf 对应构建、JS/媒体依赖和 Web Workers；校验包内依赖版本、文件清单和哈希。发布到网站/替换用户游戏/上传 Release 为另一个有明确授权的动作。
- **完成条件**：下述发布门禁全部满足，兼容矩阵和发行说明一致；所有剩余限制具名，不能出现笼统的“支持 Maniac 全部功能”而无版本/平台说明。

## 6. 排期顺序与并行边界

采用串行工作包为默认执行方式，不默认委托子代理。

|里程碑|内容|可验收产物|
|---|---|---|
|M0|W00，提前开展 JS/存档/版本补证|完整缺口台账、基线检查、最小原版对照定义、明确阻断|
|M1|W01 → W02；W11 基础状态同步落地|self 与调用/返回可恢复，控制流验证通过|
|M2|W03 → W04；W05 中不依赖图像/战斗的项|数据、表达式、字符串和基础扩展逐分支闭合|
|M3|W06/W07，W05 剩余项，W08|地图/文字/战斗/媒体可观察行为和保存恢复通过|
|M4|W09/W10；W11 原版互通收口|脚本、平台功能和存档矩阵闭合|
|M5|W12|版本/平台发布门禁、回归报告和可复现包|

W09 的证据调查和引擎评估必须在 M0 开始，不能等 M4 才发现不可行。W11 贯穿全程，不能等功能完成后才补存档。独立工作可以调整先后；若今后获准并行，避免多个实现者同时修改公共解释器、共享 bank 或 liblcf 格式。

当前不估算总天数：实际子模式数量、JS 宿主接口、原版存档格式和平台差异决定工作量。M0 完成后按已确认条目估算各包，完成 M1 后用实际吞吐校正。每包记录已关闭行为分支和待验证项，不能按代码行数或 case 数汇报进度。

## 7. 验收矩阵和发布门禁

|层次|必须证明的内容|不能替代它的证据|
|---|---|---|
|契约|字段/模式、缺省、Text、消费者和生命周期有依据|反编译导出数量、文档行号有效|
|数据|输入→结果、写入顺序、别名、编码、异常正确|仅构建通过或没有警告|
|执行|指令游标、帧、调用栈、忙碌重试、钩子顺序正确|只比较最终变量值|
|存档|恢复状态与后续执行一致；三种互通方向分别验证|解析成功、只比较保存字段|
|表现|图像/字体/音画/输入及平台能力符合定义|HTTP、静态检查、无头数据断言|
|集成|真实工程关键路径和回归通过|单条指令样例或一个游戏能启动|

最小验收族包括：普通/短记录/动态尾部；每种合法局部模式；全局/self/间接引用；0/负数/边界；独立/嵌套/并行事件；空/多行/多字节文本；保存前后；正常/失败/取消/重试。按分支选择有意义的组合，避免无效排列膨胀。

最终门禁：

1. 附录全部 168 个编号已复核，每个有效子模式都关联实际验证结果；未知和平台限制不得改名为通过。
2. 原版有意义的空操作与续行也验证消费和调度；未处理命令和未处理子模式能在验证环境被统计，日志隐藏选项不能掩盖缺口。
3. 没有已知的越界、悬空状态、错误返回目的地、错误文件写出或存档静默丢失；安全偏差明确记录。
4. 相关既有检查通过；基线失败已修复或明确阻断其影响范围，不能把没进入断言的测试记为通过。
5. 固定原版差分与各平台实际运行验收完成；随机条件、字体、解码器和设备差异可复现。
6. 原版 2000/2003 和已支持扩展回归通过；现有循环、动画、长图、Web 音频/视频工作没有被覆盖或退化。
7. 版本/平台矩阵、原版存档互通范围、依赖版本、构建产物和发行声明一致。发布批准前只准备产物，不自动部署或替换游戏程序。

## 8. 每批提交与停止条件

- 单批按行为边界提交，例如“self 读取和存储”或“调用参数及返回”；涉及 liblcf 时先提交并固定对应版本，再提交 Player 消费，双仓库版本可追溯。
- 编码前保存基线检查结果和脏文件清单。只修改该批所需部分；遇到正在变化的 Web 后端先对齐其接口，不覆盖或顺手整理无关改动。
- 每批给出：关闭的台账 ID、变更位置、契约证据、实际执行的检查、未执行验收及原因、剩余 blocker、下一批入口。
- 新发现分为“阻止本批正确性的 blocker”和“独立后续项”。修复前者；后者放回对应工作包。达到本批验收即收口，避免无限追加功能。
- 缺少验证授权时可以完成已授权设计/实现工作，但保持待验证；原版证据不足时不以猜测填充。只有所有必需项关闭才将整个兼容工程标为完成。

## 9. 全部 168 个编号的责任分配

以下是主责工作包分配，状态统一为“计划审计/补全”，不是已确认每项都缺失。每个编号只分配一个主责，跨包消费者按 W00 台账建立关联。W00 和 W12 覆盖全部；W11 覆盖所有需要持久化的状态。

|编号|名称（分析契约）|主责|关键关联|
|---:|---|---|---|
|[0](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/00000.md)|End Command|W02|按契约逐子模式复核|
|[10](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/00010.md)|End Nest|W02|按契约逐子模式复核|
|[40](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/00040.md)|Observed Opcode40|W02|按契约逐子模式复核|
|[1005](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/01005.md)|Call Common Event|W02|按契约逐子模式复核|
|[1006](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/01006.md)|Force Escape|W07|按契约逐子模式复核|
|[1007](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/01007.md)|Action Times +|W07|按契约逐子模式复核|
|[1008](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/01008.md)|Change Class|W07|按契约逐子模式复核|
|[1009](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/01009.md)|Battle Commands|W07|按契约逐子模式复核|
|[3001](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03001.md)|Get Save Info|W11|按契约逐子模式复核|
|[3002](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03002.md)|Save|W11|按契约逐子模式复核|
|[3003](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03003.md)|Load Save|W11|按契约逐子模式复核|
|[3004](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03004.md)|End Load Process|W11|关联读档标志消费者和地图更新|
|[3005](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03005.md)|Get Mouse Position|W10|按契约逐子模式复核|
|[3006](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03006.md)|Set Mouse Position|W10|按契约逐子模式复核|
|[3007](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03007.md)|Show String Picture|W06|文本切片关联 W04；self 来源关联 W01|
|[3008](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03008.md)|Get Picture Info|W06|按契约逐子模式复核|
|[3009](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03009.md)|Control Battle|W07|按契约逐子模式复核|
|[3010](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03010.md)|Control ATB Gauge|W07|按契约逐子模式复核|
|[3011](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03011.md)|Battle Command Extension|W07|按契约逐子模式复核|
|[3012](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03012.md)|Get Battle Info|W07|按契约逐子模式复核|
|[3013](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03013.md)|Control Variable Array|W03|按契约逐子模式复核|
|[3014](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03014.md)|Extended Key Input|W10|按契约逐子模式复核|
|[3015](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03015.md)|Rewrite Map|W05|按契约逐子模式复核|
|[3016](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03016.md)|Control Shared Save|W11|关联 W04 的编码及平台文件语义|
|[3017](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03017.md)|Change Picture ID|W06|按契约逐子模式复核|
|[3018](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03018.md)|Set Game Option|W08|选项与钩子关联 W06/W07/W08|
|[3019](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03019.md)|Call Command|W03|按契约逐子模式复核|
|[3020](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03020.md)|RmStrvar|W04|按契约逐子模式复核|
|[3021](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03021.md)|Get Game Info|W05|像素/执行状态分别关联 W06/W02|
|[3022](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03022.md)|Expression|W02|独立执行不求值；共享表达式属 W03|
|[3023](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03023.md)|Reserved1|W02|按契约逐子模式复核|
|[3024](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03024.md)|Reserved2|W02|按契约逐子模式复核|
|[3025](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03025.md)|Edit Picture|W06|按契约逐子模式复核|
|[3026](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03026.md)|Output Picture|W06|输出内容 W06；路径/持久化 W10|
|[3027](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03027.md)|AddAction|W05|与 11330 作为一个动作流验收|
|[3028](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03028.md)|Draw Tile On Picture|W06|按契约逐子模式复核|
|[3029](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03029.md)|Control Message|W06|按契约逐子模式复核|
|[3030](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03030.md)|JavaScript|W09|按契约逐子模式复核|
|[3031](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03031.md)|JavaScript Continuation|W09|与 3030 一起收集；直接执行另验|
|[3032](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03032.md)|Zoom Screen|W06|按契约逐子模式复核|
|[3033](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03033.md)|Console|W10|按契约逐子模式复核|
|[3034](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03034.md)|Reserved|W02|按契约逐子模式复核|
|[3035](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03035.md)|DrawShape|W02|原版不绘图；检查默认跳过时序|
|[3036](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03036.md)|Selfvar|W01|全部其他命令的 bank 访问都须联动|
|[3037](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03037.md)|Change Troop Member|W07|按契约逐子模式复核|
|[3038](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/03038.md)|Control Media Option|W08|实际窗口行为关联 W10|
|[5001](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/05001.md)|Open Load Menu|W05|按契约逐子模式复核|
|[5002](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/05002.md)|Shutdown|W02|按契约逐子模式复核|
|[5003](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/05003.md)|Toggle ATB Wait|W07|按契约逐子模式复核|
|[5004](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/05004.md)|Toggle Fullscreen|W10|按契约逐子模式复核|
|[5005](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/05005.md)|Open Video Menu|W10|按契约逐子模式复核|
|[10110](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10110.md)|Show Text|W06|按契约逐子模式复核|
|[10120](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10120.md)|Text Options|W06|按契约逐子模式复核|
|[10130](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10130.md)|Change Text Face|W06|按契约逐子模式复核|
|[10140](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10140.md)|Show Choices|W06|按契约逐子模式复核|
|[10150](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10150.md)|Input Number|W06|按契约逐子模式复核|
|[10210](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10210.md)|Control Switches|W03|按契约逐子模式复核|
|[10220](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10220.md)|ControlVariables|W03|按契约逐子模式复核|
|[10230](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10230.md)|Timer|W05|按契约逐子模式复核|
|[10310](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10310.md)|Money|W05|按契约逐子模式复核|
|[10320](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10320.md)|Item|W05|按契约逐子模式复核|
|[10330](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10330.md)|Change Party Member|W05|按契约逐子模式复核|
|[10410](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10410.md)|EXP|W05|按契约逐子模式复核|
|[10420](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10420.md)|Change Level|W05|按契约逐子模式复核|
|[10430](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10430.md)|Parameter|W05|按契约逐子模式复核|
|[10440](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10440.md)|Skill|W05|按契约逐子模式复核|
|[10450](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10450.md)|Equipment|W05|按契约逐子模式复核|
|[10460](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10460.md)|HP|W05|按契约逐子模式复核|
|[10470](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10470.md)|MP|W05|按契约逐子模式复核|
|[10480](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10480.md)|Change State|W05|按契约逐子模式复核|
|[10490](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10490.md)|Recover All|W05|按契约逐子模式复核|
|[10500](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10500.md)|Damage Processing|W05|按契约逐子模式复核|
|[10610](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10610.md)|Change Actor Name|W05|按契约逐子模式复核|
|[10620](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10620.md)|Change Actor Nickname|W05|按契约逐子模式复核|
|[10630](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10630.md)|Change Actor Graphic|W05|按契约逐子模式复核|
|[10640](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10640.md)|Change Actor Face|W05|按契约逐子模式复核|
|[10650](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10650.md)|Change Vehicle Graphic|W05|按契约逐子模式复核|
|[10660](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10660.md)|Change System BGM|W08|按契约逐子模式复核|
|[10670](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10670.md)|Change System Sound|W08|按契约逐子模式复核|
|[10680](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10680.md)|Change System Graphic|W05|按契约逐子模式复核|
|[10690](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10690.md)|Set Transition|W05|按契约逐子模式复核|
|[10710](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10710.md)|Battle|W07|按契约逐子模式复核|
|[10720](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10720.md)|Shop|W05|按契约逐子模式复核|
|[10730](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10730.md)|Inn|W05|按契约逐子模式复核|
|[10740](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10740.md)|Name Entry|W05|按契约逐子模式复核|
|[10810](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10810.md)|Transfer Player|W05|按契约逐子模式复核|
|[10820](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10820.md)|Store Place|W05|按契约逐子模式复核|
|[10830](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10830.md)|Restore Place|W05|按契约逐子模式复核|
|[10840](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10840.md)|Ride Vehicle|W05|按契约逐子模式复核|
|[10850](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10850.md)|Set Vehicle Location|W05|按契约逐子模式复核|
|[10860](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10860.md)|Set Event Location|W05|保留并复核已有特殊模式兼容|
|[10870](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10870.md)|Swap Event Positions|W05|按契约逐子模式复核|
|[10910](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10910.md)|Get Terrain ID|W05|按契约逐子模式复核|
|[10920](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/10920.md)|Get Event ID|W05|按契约逐子模式复核|
|[11010](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11010.md)|Hide Screen|W06|按契约逐子模式复核|
|[11020](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11020.md)|Show Screen|W06|按契约逐子模式复核|
|[11030](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11030.md)|Tint Screen|W06|按契约逐子模式复核|
|[11040](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11040.md)|Flash Screen|W06|按契约逐子模式复核|
|[11050](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11050.md)|Shake Screen|W06|按契约逐子模式复核|
|[11060](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11060.md)|Scroll Screen|W06|按契约逐子模式复核|
|[11070](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11070.md)|Weather Effects|W06|按契约逐子模式复核|
|[11110](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11110.md)|Show Picture|W06|按契约逐子模式复核|
|[11120](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11120.md)|Move Picture|W06|按契约逐子模式复核|
|[11130](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11130.md)|Erase Picture|W06|按契约逐子模式复核|
|[11210](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11210.md)|Show Animation|W06|保留现有动画缓冲及存档扩展|
|[11310](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11310.md)|Set Player Transparent|W05|按契约逐子模式复核|
|[11320](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11320.md)|Flash Event|W05|按契约逐子模式复核|
|[11330](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11330.md)|SetAction|W05|连续 3027、字节流及动作执行一并验收|
|[11340](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11340.md)|Wait for All Movement|W05|按契约逐子模式复核|
|[11350](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11350.md)|Stop All Movement|W05|按契约逐子模式复核|
|[11410](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11410.md)|Wait|W02|按契约逐子模式复核|
|[11510](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11510.md)|Play BGM|W08|按契约逐子模式复核|
|[11520](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11520.md)|Fadeout BGM|W08|按契约逐子模式复核|
|[11530](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11530.md)|Store BGM|W08|按契约逐子模式复核|
|[11540](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11540.md)|Replay BGM|W08|按契约逐子模式复核|
|[11550](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11550.md)|Play SE|W08|按契约逐子模式复核|
|[11560](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11560.md)|Play Movie|W08|按契约逐子模式复核|
|[11610](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11610.md)|Input Key|W10|按契约逐子模式复核|
|[11710](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11710.md)|Change Tileset|W05|按契约逐子模式复核|
|[11720](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11720.md)|Change Parallax|W05|按契约逐子模式复核|
|[11740](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11740.md)|Set Encounter Rate|W05|按契约逐子模式复核|
|[11750](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11750.md)|Replace Tile|W05|按契约逐子模式复核|
|[11810](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11810.md)|Teleport Point|W05|按契约逐子模式复核|
|[11820](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11820.md)|Teleport Permission|W05|按契约逐子模式复核|
|[11830](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11830.md)|Set Escape Position|W05|按契约逐子模式复核|
|[11840](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11840.md)|Change Escape Access|W05|按契约逐子模式复核|
|[11910](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11910.md)|Open Save Menu / Menu Operations|W05|存取档关联 W11；媒体菜单关联 W10|
|[11930](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11930.md)|Save Permission|W05|按契约逐子模式复核|
|[11950](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11950.md)|Menu|W05|按契约逐子模式复核|
|[11960](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/11960.md)|Menu Permission|W05|按契约逐子模式复核|
|[12010](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12010.md)|Conditional Branch|W02|按契约逐子模式复核|
|[12110](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12110.md)|Label|W02|按契约逐子模式复核|
|[12120](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12120.md)|Jump to Label|W02|按契约逐子模式复核|
|[12210](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12210.md)|Loop|W02|按契约逐子模式复核|
|[12220](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12220.md)|Break or Continue Loop|W02|保留并复核已有 continue/层级修复|
|[12310](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12310.md)|Exit Event Processing|W02|按契约逐子模式复核|
|[12320](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12320.md)|Erase or Restore Event|W02|按契约逐子模式复核|
|[12330](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12330.md)|CallEvent|W02|调用者/被调用者 self 关联 W01|
|[12410](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12410.md)|Comment|W02|按契约逐子模式复核|
|[12420](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12420.md)|Game Over|W02|按契约逐子模式复核|
|[12510](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/12510.md)|To Title|W02|按契约逐子模式复核|
|[13110](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/13110.md)|Change Enemy HP|W07|按契约逐子模式复核|
|[13120](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/13120.md)|Change Enemy MP|W07|按契约逐子模式复核|
|[13130](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/13130.md)|Change Enemy State|W07|按契约逐子模式复核|
|[13150](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/13150.md)|Enemy Appearance|W07|按契约逐子模式复核|
|[13210](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/13210.md)|Change Battle Background|W07|按契约逐子模式复核|
|[13260](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/13260.md)|Show Battle Animation|W07|按契约逐子模式复核|
|[13310](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/13310.md)|Battle Conditional Branch|W07|按契约逐子模式复核|
|[13410](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/13410.md)|Abort Battle|W07|按契约逐子模式复核|
|[20110](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20110.md)|Message Continuation|W06|按契约逐子模式复核|
|[20140](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20140.md)|Choice Branch|W06|按契约逐子模式复核|
|[20141](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20141.md)|End Choices|W06|按契约逐子模式复核|
|[20710](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20710.md)|If Win|W07|按契约逐子模式复核|
|[20711](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20711.md)|If Escape|W07|按契约逐子模式复核|
|[20712](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20712.md)|If Lose|W07|按契约逐子模式复核|
|[20713](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20713.md)|End Battle|W07|按契约逐子模式复核|
|[20720](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20720.md)|If Purchased|W05|按契约逐子模式复核|
|[20721](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20721.md)|If Not Purchased|W05|按契约逐子模式复核|
|[20722](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20722.md)|End Shop|W05|按契约逐子模式复核|
|[20730](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20730.md)|If Stayed|W05|按契约逐子模式复核|
|[20731](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20731.md)|If Not Stayed|W05|按契约逐子模式复核|
|[20732](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/20732.md)|End Inn|W05|按契约逐子模式复核|
|[22010](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/22010.md)|Else|W02|按契约逐子模式复核|
|[22011](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/22011.md)|End If|W02|按契约逐子模式复核|
|[22210](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/22210.md)|End Loop|W02|按契约逐子模式复核|
|[22410](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/22410.md)|Comment Continuation|W02|按契约逐子模式复核|
|[23310](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/23310.md)|Else Battle|W07|按契约逐子模式复核|
|[23311](../../RPGRewriter-maniac/analysis/Maniacs-Complete/commands/23311.md)|End If Battle|W07|按契约逐子模式复核|

编号分配校验：168 项、168 个唯一编号，与报告目录完全相等。分配数量：W01 1 项；W02 26 项；W03 4 项；W04 1 项；W05 60 项；W06 27 项；W07 25 项；W08 10 项；W09 2 项；W10 7 项；W11 5 项。W00/W12 为全局工作包，不重复占用编号。

