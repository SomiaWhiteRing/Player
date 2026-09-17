# 2026.9.1

- Android 支持从 VIPRPG 归档站导入游戏：确认后下载、逐文件 SHA-256 校验、保存 ZIP 到 Games；支持进度、取消与重复导入保护，不覆盖已有游戏和存档。
- 导入适用于 RPG Maker 2000／2003（含 Maniac）快照，最大 1 GiB、50,000 个文件；网站部署对应按钮和接口后可用。
- Android 应用显示的版本号读取本文件，继续保留递增的构建版本代码。
- Android APK 压缩原生库，保留四种 CPU 架构，下载体积降至约 57 MB；构建时强制检查小于 100 MB。
- 同时提供 Windows x64、Web 和 Android 下载。
- 使用本文件维护版本号和当前版本日志；每次发布分支提交均更新 Nightly 和本版本。
- 发布产物附带提交 SHA、构建链接、文件大小和 SHA-256，方便核对来源。

Android 包沿用现有 debug 签名。Web 部署须包含 ZIP 内的 HTML、JS、WASM 和 DATA 文件。
