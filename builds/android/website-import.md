# 网站发起游戏导入

Android 包 `org.easyrpg.player.kai` 接收 `ACTION_VIEW`：

```text
easyrpg-kai://import?manifest=<URL 编码的 HTTPS 清单地址>
```

网站使用 Android 浏览器 Intent URL，指定 `package=org.easyrpg.player.kai`，并将 `S.browser_fallback_url` 指向网站的 `/resources#easyrpg-kai`。此处是自定义协议，不依赖 Android App Links 的域名验证。仅 Android 设备上的受支持作品显示按钮；浏览器不支持 Intent 或用户未安装时，页面还保留安装／更新入口和普通 ZIP 下载。

清单固定为 `/api/archive-versions/{id}/kai-import`，当前信任域名：

- `https://viprpg-zh-archive.q578235562.workers.dev`
- `https://staging.viprpg.org`

客户端拒绝其他来源、非 HTTPS、重定向、用户信息及非默认端口。独立预生产域名需要安装已更新来源白名单的新版 APK，旧版会拒绝导入。更换站点域名时，需要同步修改网站按钮和 Android 白名单并发版。

清单格式：

```json
{
  "schema": "viprpg-kai.import.v1",
  "archiveVersionId": 123,
  "workId": 456,
  "coverUrl": "https://viprpg-zh-archive.q578235562.workers.dev/api/media/blobs/<封面 SHA-256>",
  "title": "游戏名称",
  "engineFamily": "rpg_maker_2003",
  "manifestSha256": "64 位小写十六进制哈希",
  "downloadUrl": "https://viprpg-zh-archive.q578235562.workers.dev/api/archive-versions/123/download?zip_builder=zip-store-v7-local-crc-no-descriptor",
  "zipSizeBytes": 123456,
  "files": [{ "path": "RPG_RT.ldb", "size": 1234, "sha256": "64 位小写十六进制哈希" }]
}
```

示例省略其余游戏文件；实际必须包含根目录的 `RPG_RT.ldb` 和 `RPG_RT.lmt`。清单最多 8 MiB、50,000 文件，ZIP 和文件总大小均最多 1 GiB。接受 2000、2003、2003 Maniac；具体补丁兼容性仍受 Kai 引擎能力限制。网站仅为已发布作品的已发布当前快照提供清单和下载，不需要账户凭据。GitHub 不参与游戏导入。

`workId` 用于识别同一作品，`coverUrl` 为同源 `/api/media/blobs/{sha256}` 图片地址，无封面时为 `null`。封面读取限制为 8 MiB，解码缩放到最长边 1024 像素以内；无封面或加载失败时显示占位图。

客户端验证下载响应的 `X-Manifest-SHA256`、`X-Download-Zip-Builder`、精确 `Content-Length`，再验证 ZIP 的全部条目、大小和 SHA-256；当前协议只接受网站生成的 UTF-8 STORE ZIP。拒绝额外／重复条目、大小写冲突、路径穿越和异常文件名。清单中的 manifestSha256 标识网站原始归档清单，不是 ZIP 的哈希；ZIP 内容由逐文件哈希校验。

确认页仅在窗口标题显示“从VIPRPG.org导入游戏”，正文显示适中封面、游戏名和“文件大小”，不显示预发布站标注。快照编号、原始域名和保存文件名保留在诊断日志，页面不显示后台下载说明。关闭与导入按钮位于同一行，导入为主要操作；“关闭”保留原文案，点击后返回游戏列表。未设置或无法访问游戏目录时，提示并提供“选择文件夹”。通过实际游戏文件名、同站点作品 ID 的安装记录检查游戏是否已安装；同一作品已有下载任务时也阻止重复创建。已有游戏或已有任务隐藏文件大小等附加信息，只显示封面、名称、状态和关闭按钮。用户手动改名且没有匹配安装记录的游戏无法可靠识别。

确认后立即进入游戏列表，先下载到应用私有持久目录，校验通过才复制到用户授权的 `EasyRPG/games` 下隐藏的 `.kai-import-*.part` 文件。复制后再次读取校验整个 ZIP，最终重命名为 `VIPRPG-{id}-{manifest哈希前16位}.zip`，预发布站使用 `VIPRPG-staging-` 前缀。Kai 已有 ZIP 运行能力，导入无需解压。目标已存在时不覆盖；已安装同一作品时不再导入新版快照。文件仍沿用 Kai 的独立 `saves/{ZIP文件名去扩展名}` 存档规则，不迁移或合并旧存档。

首次使用复用系统文件夹授权与 EasyRPG 文件夹检查；选择完成后再次检测本地并确认下载。未完成任务置于游戏列表顶部，显示封面、下载百分比、已下载／总大小和暂停／继续按钮，启动与游戏设置不可用；失败后按钮显示“重试”。列表与通知仅在下载阶段显示速度，暂停、排队时隐藏速度。下载后的文件校验、SAF 复制及回读校验统一显示“正在准备游戏…”和不定进度，不将处理进度重复显示为下载百分比。原子重命名成功后自动刷新为可启动的游戏。暂停或失败后可以移除下载，仅清理该任务的临时文件。

后台下载由 `dataSync` 前台服务执行，通知中显示进度及暂停入口。离开确认页、切换应用或锁屏后可继续；队列和分段 ZIP 保存在应用私有持久目录，系统重启服务或再次打开列表可恢复未暂停的任务，暂停任务保持暂停。系统强行停止应用、重启设备或撤回存储授权时不能保证持续执行；重新打开应用后可恢复。Android 15 及以上后台数据同步服务达到系统时限时保存进度并暂停。[Android 服务类型](https://developer.android.com/develop/background-work/services/fgs/service-types)、[超时限制](https://developer.android.com/develop/background-work/services/fgs/timeout)。

下载接口接受单段 HTTP `Range`，续传核对 `206` 的 `Content-Range`、剩余长度和清单身份。网站按同一 STORE ZIP 的字节偏移生成响应，跳过范围外的完整游戏文件，不将部分响应写入完整 ZIP 缓存。服务器不支持 Range 而返回 `200` 时安全地从头下载，不向旧分段追加。未完成的 SAF 文件按任务单独记录并清理，不清除其他下载的分段。

验证范围：Android Java／资源编译与 Lint，网站接口语法、ESLint 和差异检查。3 处既有 Lint 报错已处理：主题属性改用 styleable、明确校验并持久化读写授权、新版返回回调覆盖下的旧系统按键分支使用局部例外。未添加测试代码，也未做设备、模拟器或浏览器交互验收。本次版本号保持 2026.9.2，仅更新 Nightly，已发布正式版保持不变；网站接口通过其既有部署流程交付。
