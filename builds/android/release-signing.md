# Android Release 签名

自动发版和 `builds/package-docker.ps1 android` 使用 `assembleRelease`，关闭 Java 与原生调试，保留四种 CPU 架构。APK 文件名为 `EasyRPG-Player-Kai-nightly-android.apk`；编号正式版会将 `nightly` 替换为版本号。

## CI 配置

在 Player 仓库的 GitHub Actions Secrets 中配置：

| Secret | 内容 |
| --- | --- |
| `ANDROID_RELEASE_KEYSTORE_BASE64` | 正式 keystore 文件的 Base64 编码 |
| `ANDROID_RELEASE_STORE_PASSWORD` | keystore 密码 |
| `ANDROID_RELEASE_KEY_ALIAS` | 签名密钥别名 |
| `ANDROID_RELEASE_KEY_PASSWORD` | 签名密钥密码 |

Gradle 先生成未签名的 Release APK；签名环境变量不会传入 Gradle。`builds/ci/sign-android-release.sh` 将 keystore 解码到容器临时目录，用 Android SDK 的 `apksigner` 签名并验证，结束时清理临时文件。签名材料不会写入仓库、构建缓存或上传产物。流水线还会检查 APK 未启用调试、ZIP 对齐、四种架构、内置音色库、原生库压缩和签名后的 100 MB 大小限制。

签名备份保存在维护者的私有仓库 `SomiaWhiteRing/VIPRPG-Android-Signing` 的 `easyrpg-player-kai/` 目录，与网站 Android 应用的签名相互独立。密码另外保存在该私有仓库与 Player 仓库的 Actions Secrets 中。

## 本地打包

从安全位置读取 keystore 和密码，设置同名环境变量后运行：

```powershell
$env:ANDROID_RELEASE_KEYSTORE_BASE64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($keystorePath))
$env:ANDROID_RELEASE_STORE_PASSWORD = $credentials.storePassword
$env:ANDROID_RELEASE_KEY_ALIAS = $credentials.keyAlias
$env:ANDROID_RELEASE_KEY_PASSWORD = $credentials.keyPassword
try {
    .\builds\package-docker.ps1 android
} finally {
    Remove-Item Env:ANDROID_RELEASE_KEYSTORE_BASE64, Env:ANDROID_RELEASE_STORE_PASSWORD, Env:ANDROID_RELEASE_KEY_ALIAS, Env:ANDROID_RELEASE_KEY_PASSWORD
}
```

`$keystorePath` 是私有备份中的 `release.p12` 路径；`$credentials` 可从备份目录内未跟踪的 `credentials.json` 读取。不要把实际密码写进 PowerShell 命令历史或公开仓库。单独调用 Gradle 时，未设置 `RELEASE_STORE_FILE` 会生成未签名 APK；也可以用 `-PandroidUnsignedRelease=true` 强制不签名。开发用 `assembleDebug` 仍使用原来的调试密钥。

## 从旧 Debug 版迁移

新的正式签名与旧版公开的 nightly 调试签名不同，不能直接覆盖安装旧版。请先备份游戏存档及需要保留的数据，再卸载旧版并安装新版；安装后重新授权游戏目录。后续 Release 必须继续使用同一份正式密钥，才能正常覆盖更新。

正式证书 SHA-256：`ad31e0af03309667c94b58ab70847ac8069ee6b77bd90fef43ec191d153d4e78`。
