# 网站发起游戏导入

Android 包 `org.easyrpg.player.kai` 接收 `ACTION_VIEW`：

```text
easyrpg-kai://import?manifest=<URL 编码的 HTTPS 清单地址>
```

网站使用 Android 浏览器 Intent URL，指定 `package=org.easyrpg.player.kai`，并将 `S.browser_fallback_url` 指向网站的 `/resources#easyrpg-kai`。此处是自定义协议，不依赖 Android App Links 的域名验证。仅 Android 设备上的受支持作品显示按钮；浏览器不支持 Intent 或用户未安装时，页面还保留安装／更新入口和普通 ZIP 下载。

清单固定为 `/api/archive-versions/{id}/kai-import`，当前信任域名：

- `https://viprpg-zh-archive.q578235562.workers.dev`
- `https://viprpg-zh-archive-staging.q578235562.workers.dev`

客户端拒绝其他来源、非 HTTPS、重定向、用户信息及非默认端口。以后启用独立域名时，需要同步修改网站按钮和 Android 白名单并发版。

清单格式：

```json
{
  "schema": "viprpg-kai.import.v1",
  "archiveVersionId": 123,
  "title": "游戏名称",
  "engineFamily": "rpg_maker_2003",
  "manifestSha256": "64 位小写十六进制哈希",
  "downloadUrl": "https://viprpg-zh-archive.q578235562.workers.dev/api/archive-versions/123/download?zip_builder=zip-store-v7-local-crc-no-descriptor",
  "zipSizeBytes": 123456,
  "files": [{ "path": "RPG_RT.ldb", "size": 1234, "sha256": "64 位小写十六进制哈希" }]
}
```

示例省略其余游戏文件；实际必须包含根目录的 `RPG_RT.ldb` 和 `RPG_RT.lmt`。清单最多 8 MiB、50,000 文件，ZIP 和文件总大小均最多 1 GiB。接受 2000、2003、2003 Maniac；具体补丁兼容性仍受 Kai 引擎能力限制。网站仅为已发布作品的已发布当前快照提供清单和下载，不需要账户凭据。GitHub 不参与游戏导入。

客户端验证下载响应的 `X-Manifest-SHA256`、精确 `Content-Length`，再验证 ZIP 的全部条目、大小和 SHA-256；当前协议只接受网站生成的 UTF-8 STORE ZIP。拒绝额外／重复条目、大小写冲突、路径穿越和异常文件名。清单中的 manifestSha256 标识网站原始归档清单，不是 ZIP 的哈希；ZIP 内容由逐文件哈希校验。

确认后先下载到应用私有缓存，校验通过才复制到用户授权的 `EasyRPG/games` 下隐藏的 `.kai-import-*.part` 文件。复制后再次读取校验整个 ZIP，最终重命名为 `VIPRPG-{id}-{manifest哈希前16位}.zip`，预发布站使用 `VIPRPG-staging-` 前缀。Kai 已有 ZIP 运行能力，导入无需解压。目标已存在时不覆盖；新版快照单独保存，沿用 Kai 的独立 `saves/{ZIP文件名去扩展名}` 存档规则，不迁移或合并旧存档。

首次使用复用系统文件夹授权与 EasyRPG 文件夹检查；选择完成后再次确认下载。ViewModel 保留旋转时的任务、进度及取消状态，独立 Activity 任务避免破坏正在运行的游戏。用户应保持导入页面开启；这不是持久后台下载服务，不支持断点续传。进程被系统结束后不自动恢复下载，下次导入清理记录中的未完成文件和私有缓存，重新确认。存储授权被撤回或存储不可用时可能需要手动清理隐藏的 `.part` 文件；不删除任何已完成游戏。

验证范围：运行现有 Android 编译／打包和网站静态检查。未添加测试代码，也未做设备、模拟器或浏览器交互验收。
