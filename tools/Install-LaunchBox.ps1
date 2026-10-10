#Requires -Version 7.0
<#
.SYNOPSIS
    Installs a built cannonball-se.exe (and the runtime DLLs it was built with) into the LaunchBox Racing copy, with a
    backup and the rig-profile controls capability receipt; or restores a backup.

.DESCRIPTION
    Only the runtime files are replaced:
      - cannonball-se.exe;
      - each DLL in the build folder whose bytes differ, or which is missing (SDL2, ANGLE, mpg123, tinyxml2, zlib,
        WheelFfb).
    config.xml, res\, roms\ and every other file are never touched. The replaced files go to
    _backup-<utc>-<label>\ first, with backup.json naming the source commit and every hash.
    dbce-cannonball-controls.json (adapter cannonball-xml-1) declares the controls capability Wheelkit's
    CannonBallControlProfile checks: schema 1, the exe's SHA-256, the source commit. Refuses while CannonBall is
    running, on a dirty source tree, and on a build not made from HEAD's runtime sources.

.EXAMPLE
    .\tools\Install-LaunchBox.ps1 -BuildDir build-claude\Release -Label test-injection
    .\tools\Install-LaunchBox.ps1 -Restore "<Target>\_backup-20261010-110000-test-injection"
#>
[CmdletBinding()]
param(
    [string]$BuildDir,
    [string]$Target = 'E:\Source\toolkits\launchbox\Launchbox-Racing\Games\Windows\OutRun (Cannonball-SE)',
    [ValidatePattern('^[a-z0-9][a-z0-9-]*$')][string]$Label,
    [string]$Restore
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$exe = 'cannonball-se.exe'
$receiptName = 'dbce-cannonball-controls.json'
function Sha([string]$p) { (Get-FileHash -LiteralPath $p).Hash }
if (Get-Process 'cannonball-se' -ErrorAction SilentlyContinue) { throw 'CannonBall is running; nothing changed.' }
$Target = (Resolve-Path -LiteralPath $Target).Path
if ($Restore) {
    $Restore = (Resolve-Path -LiteralPath $Restore).Path
    $manifest = Get-Content -LiteralPath (Join-Path $Restore 'backup.json') -Raw | ConvertFrom-Json
    if ($manifest.target -ne $Target) { throw 'This backup belongs to another folder.' }
    foreach ($f in $manifest.files) {
        $dest = Join-Path $Target $f.name
        if ($f.existed) {
            if ((Sha (Join-Path $Restore $f.name)) -ne $f.sha256) { throw "Backup hash mismatch: $($f.name)" }
            Copy-Item -LiteralPath (Join-Path $Restore $f.name) -Destination $dest -Force
        } elseif (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest }
    }
    "restored $($manifest.files.Count) file(s) from $Restore"
    return
}
if (-not $BuildDir -or -not $Label) { throw 'Give -BuildDir and -Label (or -Restore).' }
$BuildDir = (Resolve-Path -LiteralPath (Join-Path $repo $BuildDir)).Path
if (git -C $repo status --porcelain -- src CMakeLists.txt) { throw 'The runtime sources have uncommitted changes; build and install from a commit.' }
$commit = (git -C $repo rev-parse HEAD).Trim()
$built = Join-Path $BuildDir $exe
if (-not (Test-Path -LiteralPath $built)) { throw "No $exe in $BuildDir." }
$src = @(Get-ChildItem (Join-Path $repo 'src') -Recurse -File | Where-Object { $_.LastWriteTimeUtc -gt (Get-Item $built).LastWriteTimeUtc })
if ($src.Count) { throw "The build is older than $($src.Count) source file(s) (e.g. $($src[0].Name)); rebuild first." }
$names = @($exe) + @(Get-ChildItem $BuildDir -Filter *.dll -File | Select-Object -ExpandProperty Name)
$replace = @($names | Where-Object { -not (Test-Path -LiteralPath (Join-Path $Target $_)) -or (Sha (Join-Path $Target $_)) -ne (Sha (Join-Path $BuildDir $_)) })
$replace += $receiptName
$backup = Join-Path $Target ("_backup-" + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + "-$Label")
New-Item -ItemType Directory -Path $backup | Out-Null
$files = foreach ($n in $replace) {
    $dest = Join-Path $Target $n
    $existed = Test-Path -LiteralPath $dest
    if ($existed) { Copy-Item -LiteralPath $dest -Destination (Join-Path $backup $n); if ((Sha (Join-Path $backup $n)) -ne (Sha $dest)) { throw "Backup verification failed: $n" } }
    [ordered]@{ name = $n; existed = $existed; sha256 = $(if ($existed) { Sha $dest } else { $null }) }
}
[ordered]@{ schemaVersion = 1; target = $Target; sourceCommit = $commit; label = $Label; createdUtc = [DateTime]::UtcNow.ToString('o'); files = @($files) } |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $backup 'backup.json') -Encoding utf8
try {
    foreach ($n in $replace) { if ($n -ne $receiptName) { Copy-Item -LiteralPath (Join-Path $BuildDir $n) -Destination (Join-Path $Target $n) -Force } }
    $receipt = [ordered]@{ schemaVersion = 1; controlsProfileSchema = 1; adapter = 'cannonball-xml-1'; executable = $exe; sha256 = (Sha (Join-Path $Target $exe));
        sourceCommit = $commit; label = $Label; installedUtc = [DateTime]::UtcNow.ToString('o'); backup = (Split-Path $backup -Leaf) }
    [IO.File]::WriteAllText((Join-Path $Target $receiptName), ($receipt | ConvertTo-Json), [Text.UTF8Encoding]::new($false))
    foreach ($n in $replace) { if ($n -ne $receiptName -and (Sha (Join-Path $Target $n)) -ne (Sha (Join-Path $BuildDir $n))) { throw "Readback failed: $n" } }
} catch {
    & $PSCommandPath -Restore $backup -Target $Target | Out-Null
    throw "Install failed and was rolled back: $($_.Exception.Message)"
}
"installed $($replace.Count - 1) runtime file(s) from $commit; receipt $receiptName; backup $(Split-Path $backup -Leaf)"
