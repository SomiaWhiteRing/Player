# 2026.9.1

- Android APK 压缩原生库，保留四种 CPU 架构，下载体积降至约 57 MB；构建时强制检查小于 100 MB。
- 同时提供 Windows x64、Web 和 Android 下载。
- 使用本文件维护版本号和当前版本日志；每次发布分支提交均更新 Nightly 和本版本。
- 发布产物附带提交 SHA、构建链接、文件大小和 SHA-256，方便核对来源。

Android 包沿用现有 debug 签名。Web 部署须包含 ZIP 内的 HTML、JS、WASM 和 DATA 文件。
