param(
    [Parameter(Mandatory)][string]$StageRoot,
    [Parameter(Mandatory)][string]$ContractPath,
    [string]$SourceInventoryPath = (Join-Path $PSScriptRoot 'source-inventory.d128256.json')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Read-only gate. The reviewed contract must be supplied independently of the stage.
function Fail([string]$Message) { throw "Release stage rejected: $Message" }
function CheckPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path) -or $Path -match '(^/|\\|:|[<>"|?*\x00-\x1F]|//|/$)' -or
        @($Path.Split('/') | Where-Object { $_ -in @('.','..') -or $_ -match '[ .]$' }).Count) {
        Fail "unsafe relative path: $Path"
    }
    foreach ($part in $Path.Split('/')) {
        if ($part -match '^(roms|run|userdata|saves|\.git|\.env|credentials)(\.|$)' -or
            $part -match '^(epr|mpr)[-_]' -or $part -match '^(hiscores.*|play_stats)\.' -or
            $part -match '\.(zip|7z|rar|tar|gz|mp3|wav|ym|rom|ini|log)$' -or
            $part -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)') {
            Fail "private/ROM/archive path is disallowed: $Path"
        }
    }
    if ($Path -match '(^|/)config\.xml$' -and $Path -notin @('defaults/config.xml','source/res/config.xml')) {
        Fail 'personal configuration is disallowed; only reviewed defaults are accepted'
    }
}
function HashFile([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Descendants([string]$Directory) {
    foreach ($item in Get-ChildItem -LiteralPath $Directory -Force) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { Fail "links/reparse points are disallowed: $($item.Name)" }
        CheckPath ([IO.Path]::GetRelativePath($stage,$item.FullName).Replace('\','/'))
        if ($item.PSIsContainer) { Descendants $item.FullName } else { $item }
    }
}
$stage = [IO.Path]::GetFullPath($StageRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
$contractFile = [IO.Path]::GetFullPath($ContractPath)
if (-not (Test-Path -LiteralPath $stage -PathType Container)) { Fail 'stage directory is missing' }
# Check ancestors too: do not traverse a stage through a junction/symlink.
$ancestor = Get-Item -LiteralPath $stage -Force
while ($null -ne $ancestor) {
    if ($ancestor.Attributes -band [IO.FileAttributes]::ReparsePoint) { Fail 'stage ancestry contains a reparse point' }
    $ancestor = $ancestor.Parent
}
if ($contractFile.StartsWith($stage + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
    Fail 'contract must be external to the untrusted stage'
}
if ((Get-Item -LiteralPath $contractFile -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { Fail 'contract is a link' }
$utf8 = [Text.UTF8Encoding]::new($false,$true)
$contract = $utf8.GetString([IO.File]::ReadAllBytes($contractFile)) | ConvertFrom-Json
$sourceInventoryFile = [IO.Path]::GetFullPath($SourceInventoryPath)
if ($sourceInventoryFile.StartsWith($stage + [IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
    ((Get-Item -LiteralPath $sourceInventoryFile -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { Fail 'source inventory must be an independent reviewed file' }
$approvedSource = $utf8.GetString([IO.File]::ReadAllBytes($sourceInventoryFile)) | ConvertFrom-Json
if ($approvedSource.schema -ne 'dbce-outrun-arcade-source-inventory-v1' -or $approvedSource.repository -ne $contract.repository -or
    $approvedSource.commit -ne $contract.source.commit -or $approvedSource.tree -ne $contract.source.tree) { Fail 'source snapshot identity differs from reviewed inventory' }
if ($contract.schema -ne 'dbce-outrun-arcade-stage-v1' -or $contract.repository -ne 'd-b-c-e/dbce-mods-outrun-arcade') { Fail 'wrong game/product contract' }
if ($contract.source.commit -notmatch '^[a-f0-9]{40}$' -or $contract.source.tree -notmatch '^[a-f0-9]{40}$' -or
    $contract.build.source_commit -notmatch '^[a-f0-9]{40}$' -or $contract.build.architecture -ne 'x64' -or
    $contract.build.configuration -ne 'Release' -or [string]::IsNullOrWhiteSpace($contract.build.compiler) -or
    [string]::IsNullOrWhiteSpace($contract.build.sdk) -or @($contract.build.configure_arguments).Count -eq 0) { Fail 'incomplete source/build identity' }
$expected = [Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
$roles = @('runtime','resource','source','dependency-source','notice','documentation','default-config')
foreach ($entry in $contract.files) {
    CheckPath $entry.path
    if ($expected.ContainsKey($entry.path)) { Fail 'duplicate/case-colliding allowlist entry' }
    if ($entry.sha256 -notmatch '^[a-f0-9]{64}$' -or $entry.bytes -lt 1 -or $entry.bytes -ne [math]::Floor($entry.bytes) -or $entry.role -notin $roles) { Fail "invalid file identity: $($entry.path)" }
    $expected.Add($entry.path,$entry)
}
$runtime = @('cannonball-se.exe','SDL2.dll','tinyxml2.dll','libEGL.dll','libGLESv2.dll','mpg123.dll','zlib1.dll')
foreach ($path in $runtime) { if (-not $expected.ContainsKey($path) -or $expected[$path].role -ne 'runtime') { Fail "missing runtime identity: $path" } }
if (@($contract.files | Where-Object role -EQ 'runtime').Count -ne $runtime.Count) { Fail 'unexpected runtime artifact' }
$resources = @('res/gamecontrollerdb.txt','res/tilemap.bin','res/tilepatch.bin','res/Cannonball-Shader-Vertex.glsl','res/Cannonball-Shader-Fragment.glsl','res/Cannonball-Shader-Fragment-Fast.glsl')
foreach ($path in $resources) { if (-not $expected.ContainsKey($path) -or $expected[$path].role -ne 'resource') { Fail "missing default-path resource: $path" } }
if (@($contract.files | Where-Object role -EQ 'resource').Count -ne $resources.Count) { Fail 'unexpected runtime resource' }
foreach ($name in @('license.txt','CannonBall-SE-license.txt','THIRD-PARTY-NOTICES.md','LGPL-2.1.txt','license_mame.txt','license_atari800.txt')) {
    $path = 'notices/' + $name
    if (-not $expected.ContainsKey($path) -or $expected[$path].role -ne 'notice') { Fail "missing notice: $name" }
}
if (-not $expected.ContainsKey('docs/OUTRUN-ARCADE-SETUP.md') -or $expected['docs/OUTRUN-ARCADE-SETUP.md'].role -ne 'documentation') { Fail 'missing standalone setup guide' }
$inventory = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
if (@($approvedSource.files).Count -ne @($contract.source.inventory).Count) { Fail 'source inventory is incomplete' }
foreach ($item in $contract.source.inventory) {
    CheckPath $item.path
    if ($item.path.StartsWith('source/') -or $item.blob -notmatch '^[a-f0-9]{40}$' -or $inventory.ContainsKey($item.path)) { Fail 'invalid source inventory' }
    $inventory.Add($item.path,$item.blob)
    $approved = @($approvedSource.files | Where-Object path -CEQ $item.path)
    if ($approved.Count -ne 1 -or $approved[0].blob -cne $item.blob) { Fail 'source inventory differs from reviewed snapshot' }
    $path = 'source/' + $item.path
    if (-not $expected.ContainsKey($path) -or $expected[$path].role -ne 'source') { Fail "missing corresponding source: $path" }
}
foreach ($path in @('CMakeLists.txt','CMakePresets.json','vcpkg.json','vcpkg-configuration.json','src/main/main.cpp','src/main/directx/ffeedback.cpp')) {
    if (-not $inventory.ContainsKey($path)) { Fail "incomplete core/build source inventory: $path" }
}
if (@($contract.files | Where-Object role -EQ 'source').Count -ne $inventory.Count) { Fail 'source files differ from reviewed inventory' }
$components = @('sdl2','tinyxml2','mpg123','angle','zlib')
$seen = @()
foreach ($dependency in $contract.dependencies) {
    if ($dependency.name -notin $components -or $dependency.name -in $seen -or [string]::IsNullOrWhiteSpace($dependency.version) -or
        [string]::IsNullOrWhiteSpace($dependency.upstream) -or @($dependency.source_paths).Count -eq 0 -or @($dependency.notice_paths).Count -eq 0) { Fail 'incomplete dependency provenance' }
    $seen += $dependency.name
    foreach ($path in $dependency.source_paths) { if (-not $expected.ContainsKey($path) -or $expected[$path].role -ne 'dependency-source') { Fail 'missing dependency corresponding source' } }
    foreach ($path in $dependency.notice_paths) { if (-not $expected.ContainsKey($path) -or $expected[$path].role -ne 'notice') { Fail 'missing dependency notice' } }
}
if ($seen.Count -ne $components.Count) { Fail 'dependency source coverage is incomplete' }
$actual = @(Descendants $stage)
if ($actual.Count -ne $expected.Count) { Fail 'staged file count differs from exact allowlist' }
$actualNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($file in $actual) {
    $path = [IO.Path]::GetRelativePath($stage,$file.FullName).Replace('\','/')
    CheckPath $path
    if (-not $actualNames.Add($path) -or -not $expected.ContainsKey($path)) { Fail "unexpected artifact: $path" }
    $entry = $expected[$path]
    if ($file.Length -ne $entry.bytes -or (HashFile $file.FullName) -ne $entry.sha256) { Fail "size/hash mismatch: $path" }
    if ($entry.role -in @('source','resource')) {
        $bytes = [IO.File]::ReadAllBytes($file.FullName)
        $header = [Text.Encoding]::ASCII.GetBytes("blob $($bytes.Length)`0")
        $blob = [Convert]::ToHexString([Security.Cryptography.SHA1]::HashData([byte[]]($header+$bytes))).ToLowerInvariant()
        $sourcePath = if ($entry.role -eq 'source') { $path.Substring(7) } else { $path }
        if (-not $inventory.ContainsKey($sourcePath) -or $blob -ne $inventory[$sourcePath]) { Fail "source/resource Git blob mismatch: $path" }
    }
}
[pscustomobject]@{ verified=$true; repository=$contract.repository; sourceCommit=$contract.source.commit; buildSourceCommit=$contract.build.source_commit; files=$actual.Count; contractSHA256=(HashFile $contractFile); sourceInventorySHA256=(HashFile $sourceInventoryFile); runtimeExecuted=$false }
