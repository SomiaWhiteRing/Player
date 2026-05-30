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
- `ExtraHideManiacLogs`：隐藏 Maniac 相关日志

### 隐藏Maniac相关的错误提示

顾名思义。毫无意义又烦人。

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

### 兼容 `ChangePartyMember(actor 0)`

部分 RM2000 游戏会在“重建队伍”时先批量执行“移除队伍成员”，并且从 `actor 0` 开始。  
原版 `RPG_RT` 会静默忽略这条无效命令；本分支现在也按同样方式处理，不再在顶部弹出
`ChangePartyMember: Invalid actor ID 0` 警告。

已确认这能兼容 `もしもコレクション7` 一类使用该写法的工程。

## 本地打包

当前仓库在 Windows x64 下打包单文件 Release 版时使用：

```powershell
$cmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cmake --preset windows-x64-vs2022-release-local
& $cmake --build --preset windows-x64-vs2022-release-local

Remove-Item -Recurse -Force build/package/stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force build/package/stage, build/artifacts | Out-Null
Copy-Item build/windows-x64-vs2022-release-local/Release/Player.exe build/package/stage/
Compress-Archive -Path build/package/stage/* -DestinationPath build/artifacts/EasyRPG-Player-Kai-local-windows-x64.zip -Force
```

打包后的 zip 可直接分发，默认输出到 `build/artifacts/`。Release preset 使用 `x64-windows-static`，发布时只需要 `Player.exe`。

Web 版使用 Emscripten preset：

```bash
export EASYRPG_BUILDSCRIPTS=/path/to/buildscripts
source "$EASYRPG_BUILDSCRIPTS/emscripten/emsdk-portable/emsdk_env.sh"
cmake --preset emscripten-sdl3-release
cmake --build --preset emscripten-sdl3-release

bash ./builds/package-web.sh
```

Web zip 需要通过 HTTP 服务访问。游戏数据放在 `games/default/`，并使用 `resources/emscripten/indexgen.php` 生成 `index.json`。

Android 版使用 `builds/android` 下的 Gradle 工程：

```bash
cd builds/android
./gradlew -PtoolchainDirs="/path/to/buildscripts/android" assembleDebug
```

Nightly Action 上传的是 debug-signed APK，方便直接安装测试；正式 release 签名需要另行配置 keystore。
