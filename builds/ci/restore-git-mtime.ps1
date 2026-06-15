param(
	[string] $Root = $(if ($env:GITHUB_WORKSPACE) { $env:GITHUB_WORKSPACE } else { (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path }),
	[string] $ChangedSince = ""
)

$ErrorActionPreference = "Stop"

if (-not [System.IO.Path]::IsPathRooted($Root)) {
	$Root = (Resolve-Path $Root).Path
}

Push-Location $Root
try {
	$null = git rev-parse --is-inside-work-tree
	if ($LASTEXITCODE -ne 0) {
		throw "Not a git work tree: $Root"
	}

	$tracked = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
	git ls-files | ForEach-Object {
		if ($_) {
			[void] $tracked.Add($_)
		}
	}

	$currentCommitTime = $null
	$logLines = git log --name-only --format="@@PLAYER_COMMIT_TIME@@%cI" -- .
	foreach ($line in $logLines) {
		if ($line.StartsWith("@@PLAYER_COMMIT_TIME@@")) {
			$currentCommitTime = [System.DateTimeOffset]::Parse($line.Substring(22)).UtcDateTime
			continue
		}

		if (-not $line -or -not $currentCommitTime -or -not $tracked.Contains($line)) {
			continue
		}

		$path = Join-Path $Root $line
		if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
			continue
		}

		[System.IO.File]::SetLastWriteTimeUtc($path, $currentCommitTime)
		[void] $tracked.Remove($line)

		if ($tracked.Count -eq 0) {
			break
		}
	}

	if ($ChangedSince) {
		$null = git cat-file -e "$ChangedSince^{commit}" 2>$null
		if ($LASTEXITCODE -eq 0) {
			$now = [System.DateTime]::UtcNow
			$changedFiles = git diff --name-only $ChangedSince HEAD --
			foreach ($changedFile in $changedFiles) {
				if (-not $changedFile) {
					continue
				}

				$path = Join-Path $Root $changedFile
				if (Test-Path -LiteralPath $path -PathType Leaf) {
					[System.IO.File]::SetLastWriteTimeUtc($path, $now)
				}
			}
		}
	}
} finally {
	Pop-Location
}
