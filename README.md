# EasyRPG Player - Kai

EasyRPG Player 的苍旻白轮个人魔改造版。基于 EasyRPG Player 0.8.1.1。

当前维护 Windows 11 x64、Web/Emscripten 和 Android 版。
软件本身的功能请参阅[官方项目](https://easyrpg.org/)说明。

## 主要改动

### 追加功能开关

按 `F1` 打开系统设置菜单后，可以进入 `追加功能` 单独开关本分支新增功能。

这些开关默认启用，也会写入配置文件的 `[Player]` 段：

- `ExtraMessageHistory`：对话历史记录
- `ExtraMouseSupport`：鼠标功能补完
- `ExtraMoviePlayback`：兼容视频播放
- `ExtraRecommendedSoundFont`：使用作者推荐的SoundFont
- `ExtraNameInputChoices`：输入式解谜自动选项化
- `ExtraPinyinInput`：角色名输入界面追加拼音输入页
- `ExtraHideManiacLogs`：隐藏 Maniac 相关日志
- `ExtraForceTestPlay`：强制启用 EasyRPG 调试模式

### 隐藏Maniac相关的错误提示

顾名思义。毫无意义又烦人。

### 强制TestPlay

开启 `ExtraForceTestPlay` 后，会强制启用 EasyRPG 的 TestPlay / Debug 模式，效果等同于启动时传入 `--test-play`。

关闭该开关后，仅在显式使用 `--test-play`、`TestPlay` 或在游戏浏览器中以调试方式启动游戏时启用调试模式。

### 添加对话历史记录

在对话状态下，按下`~`键（ESC键下方的那个）或鼠标滚轮上滚可以打开对话历史记录界面，查看之前的对话内容。  
支持键盘和鼠标滚轮翻页。取消键和鼠标右键退出。

### 鼠标功能补完

开启 `ExtraMouseSupport` 后，在对话状态/标题界面/存读档界面下，鼠标左键能够起到和确定键相同的效果。

普通对话等待继续时，鼠标滚轮下滚也等同于确定；选项选择和数值输入不会把滚轮下滚当作确定。

### 加速倍率支持小数

`Fast Forward A/B` 的倍率现在支持一位小数，范围为 `0.1` 到 `100.0`。

小于 `1.0` 时会作为减速使用，例如 `0.1` 表示十分之一速度。

### 兼容视频播放

兼容RPG Maker事件中的播放视频功能。  
会从游戏目录下的`Movie`文件夹查找`.avi`和`.mpg`文件，并在游戏窗口内播放。  
视频播放结束后会继续执行后续事件。

### 输入式解谜自动选项化

当RPG Maker事件使用“角色改名 + 立刻判断名字”的方式做输入式解谜时，会在进入输入前自动弹出候选列表。  
可以直接从答案中选择，或选择最后一项`主动输入`回退到原本的键盘输入界面。  
适合汉化后默认选字式输入法难以覆盖答案名称的场景。

### 拼音输入法

开启 `ExtraPinyinInput` 后，简体中文的角色名输入界面会追加 `<拼音>` 页。<br>
在该页可以像常见拼音输入法一样连续输入拼音，支持单字和短语候选，例如 `nihao` 会候选 `你好`，`zhongguo` 会候选 `中国`。<br>
空格提交当前候选，`1` 到 `4` 选择当前页候选，`PageUp` / `PageDown` 翻候选页，取消键或退格键会优先删除拼音串。

拼音候选词库来自 [rime-pinyin-simp](https://github.com/rime/rime-pinyin-simp) 的袖珍简化字拼音词典，按 Apache-2.0 授权分发；许可文本见 `resources/pinyin/LICENSE-rime-pinyin-simp.txt`。

### 兼容 `ChangePartyMember(actor 0)`

部分 RM2000 游戏会在“重建队伍”时先批量执行“移除队伍成员”，并且从 `actor 0` 开始。  
原版 `RPG_RT` 会静默忽略这条无效命令；本分支现在也按同样方式处理，不再在顶部弹出
`ChangePartyMember: Invalid actor ID 0` 警告。

已确认这能兼容 `もしもコレクション7` 一类使用该写法的工程。

## 本地打包

本地标准打包入口以 Docker Desktop 为准，和 `.github/workflows/nightly-release.yml` 共用同一套仓库内脚本，不再维护旧的本地 VS / CMake 手动 zip 流程。先启动 Docker Desktop，然后在仓库根目录运行：

```powershell
.\builds\package-docker.ps1 all
```

也可以只打某个平台：

```powershell
.\builds\package-docker.ps1 web
.\builds\package-docker.ps1 android
```

脚本会先构建本地打包镜像 `easyrpg-player-kai-package:ubuntu24.04`，再把仓库挂载到容器里的 `/workspace`，调用 `builds/ci/package-ubuntu-nightly.sh` 执行和 GitHub Workflow 相同的 Web / Android Nightly 构建、校验和打包逻辑。首次构建会下载并生成 EasyRPG 工具链，时间会比较长；后续会复用 `external/local-docker/` 下的工具链缓存。

默认输出到 `build/artifacts/`：

- `EasyRPG-Player-Kai-nightly-web.zip`
- `EasyRPG-Player-Kai-nightly-android-debug.apk`

Web zip 需要通过 HTTP 服务访问。游戏数据放在 `games/default/`，并使用 `resources/emscripten/indexgen.php` 生成 `index.json`。

Windows x64 的 `Player.exe` 由 GitHub Workflow 的 `windows-2022` job 调用 `builds/ci/package-windows-nightly.ps1` 生成；它依赖 Visual Studio 2022 runner 和 `x64-windows-static` vcpkg 工具链，不在本地 Linux Docker 容器里另建一套交叉编译流程。标准分发产物以 Nightly Action 上传的 `Player.exe` 为准。

Nightly Action 上传的是 debug-signed APK，方便直接安装测试；正式 release 签名需要另行配置 keystore。
