param(
	[ValidateSet("all", "web", "android")]
	[string] $Target = "all",

	[string] $Image = "easyrpg-player-kai-package:ubuntu24.04",

	[switch] $SkipImageBuild,

	[switch] $Pull
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$dockerContext = Join-Path (Join-Path $repoRoot "builds") "docker"
$dockerfile = Join-Path $dockerContext "package.Dockerfile"
$cacheRoot = Join-Path (Join-Path $repoRoot "external") "local-docker"
$isWindowsHost = [System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT

Get-Command docker -ErrorAction Stop | Out-Null

& docker info --format "{{.ServerVersion}}" | Out-Null
if ($LASTEXITCODE -ne 0) {
	throw "Docker Desktop is not running or the docker CLI cannot reach it."
}

@(
	$cacheRoot,
	(Join-Path $cacheRoot "home"),
	(Join-Path $cacheRoot "gradle"),
	(Join-Path $cacheRoot "web"),
	(Join-Path $cacheRoot "android")
) | ForEach-Object {
	New-Item -ItemType Directory -Force -Path $_ | Out-Null
}

if (-not $SkipImageBuild) {
	$buildArgs = @("build", "-f", $dockerfile, "-t", $Image)
	if ($Pull) {
		$buildArgs += "--pull"
	}
	$buildArgs += $dockerContext

	& docker @buildArgs
	if ($LASTEXITCODE -ne 0) {
		exit $LASTEXITCODE
	}
}

$runArgs = @(
	"run",
	"--rm",
	"--init",
	"-v", "${repoRoot}:/workspace",
	"-w", "/workspace",
	"-e", "HOME=/workspace/external/local-docker/home",
	"-e", "GRADLE_USER_HOME=/workspace/external/local-docker/gradle",
	"-e", "GITHUB_RUN_NUMBER=$env:GITHUB_RUN_NUMBER",
	$Image,
	"bash",
	"./builds/ci/package-ubuntu-nightly.sh",
	$Target
)

$passthroughEnv = @(
	"WEB_ASSET_NAME",
	"ANDROID_ASSET_NAME",
	"ANDROID_ABIS",
	"ANDROID_NDK_VERSION",
	"BUILDSCRIPTS_REF",
	"VERSION_CODE_OVERRIDE"
)

foreach ($name in $passthroughEnv) {
	$value = [System.Environment]::GetEnvironmentVariable($name)
	if ($value) {
		$imageIndex = $runArgs.IndexOf($Image)
		$runArgs = $runArgs[0..($imageIndex - 1)] + @("-e", "${name}=${value}") + $runArgs[$imageIndex..($runArgs.Count - 1)]
	}
}

if (-not $isWindowsHost) {
	$userId = (& id -u).Trim()
	$groupId = (& id -g).Trim()
	$runArgs = @("run", "--rm", "--init", "--user", "${userId}:${groupId}") + $runArgs[3..($runArgs.Count - 1)]
}

& docker @runArgs
if ($LASTEXITCODE -ne 0) {
	exit $LASTEXITCODE
}
