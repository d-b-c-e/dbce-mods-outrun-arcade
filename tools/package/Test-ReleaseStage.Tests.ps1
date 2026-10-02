param([string]$FixtureRoot = (Join-Path $PSScriptRoot 'test-output'))
$ErrorActionPreference = 'Stop'
$validator = Join-Path $PSScriptRoot 'Test-ReleaseStage.ps1'
$utf8 = [Text.UTF8Encoding]::new($false,$true)
$results = @()
function Fixture([string]$Name) {
    $root = Join-Path ([IO.Path]::GetFullPath($FixtureRoot)) ($Name + '-' + [guid]::NewGuid().ToString('N'))
    $stage = Join-Path $root 'stage'
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    $files = [Collections.Generic.List[object]]::new()
    $inventory = [Collections.Generic.List[object]]::new()
    function Add([string]$Path,[string]$Role) {
        $destination = Join-Path $stage $Path
        New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
        # Text fixtures, deliberately not real runnable binaries, assets or ROMs.
        $identity = if ($Path.StartsWith('source/')) { $Path.Substring(7) } else { $Path }
        $bytes = $utf8.GetBytes("synthetic fixture: $identity`n")
        [IO.File]::WriteAllBytes($destination,$bytes)
        $files.Add(@{path=$Path;role=$Role;bytes=$bytes.Length;sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()})
        if ($Role -eq 'source') {
            $head = [Text.Encoding]::ASCII.GetBytes("blob $($bytes.Length)`0")
            $inventory.Add(@{path=$Path.Substring(7);blob=[Convert]::ToHexString([Security.Cryptography.SHA1]::HashData([byte[]]($head+$bytes))).ToLowerInvariant()})
        }
    }
    foreach ($path in @('cannonball-se.exe','SDL2.dll','tinyxml2.dll','libEGL.dll','libGLESv2.dll','mpg123.dll','zlib1.dll')) { Add $path 'runtime' }
    foreach ($path in @('license.txt','CannonBall-SE-license.txt','THIRD-PARTY-NOTICES.md','LGPL-2.1.txt','license_mame.txt','license_atari800.txt','GPL-2.0.txt','dirent-LICENSE.txt','sdl2-LICENSE.txt','tinyxml2-LICENSE.txt','mpg123-LICENSE.txt','angle-LICENSE.txt','zlib-LICENSE.txt')) { Add ('notices/'+$path) 'notice' }
    foreach ($path in @('CMakeLists.txt','CMakePresets.json','vcpkg.json','vcpkg-configuration.json','src/main/main.cpp','src/main/directx/ffeedback.cpp')) { Add ('source/'+$path) 'source' }
    foreach ($path in @('res/gamecontrollerdb.txt','res/tilemap.bin','res/tilepatch.bin','res/Cannonball-Shader-Vertex.glsl','res/Cannonball-Shader-Fragment.glsl','res/Cannonball-Shader-Fragment-Fast.glsl')) { Add $path 'resource'; Add ('source/'+$path) 'source' }
    Add 'docs/OUTRUN-ARCADE-SETUP.md' 'documentation'
    $dependencies = @()
    foreach ($name in @('sdl2','tinyxml2','mpg123','angle','zlib')) {
        $source = "dependency-source/$name/source.txt"
        $notice = "notices/$name.txt"
        Add $source 'dependency-source'
        Add $notice 'notice'
        $dependencies += @{name=$name;version='synthetic-1';upstream="https://example.invalid/$name";source_paths=@($source);notice_paths=@($notice)}
    }
    $contract = @{schema='dbce-outrun-arcade-stage-v1';repository='d-b-c-e/dbce-mods-outrun-arcade';source=@{commit='d128256afa2c9e7ac4dc164125c30e95a7d1ab3c';tree='559f22a398640674b30c1fe6950f185644486b33';inventory=$inventory.ToArray()};build=@{source_commit=('c'*40);architecture='x64';configuration='Release';compiler='synthetic-compiler';sdk='synthetic-sdk';configure_arguments=@('synthetic-argument')};dependencies=$dependencies;files=$files.ToArray()}
    $inventoryPath = Join-Path $root 'reviewed-source-inventory.json'
    @{schema='dbce-outrun-arcade-source-inventory-v1';repository=$contract.repository;commit=$contract.source.commit;tree=$contract.source.tree;files=$inventory.ToArray()} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $inventoryPath -Encoding utf8NoBOM
    @{root=$root;stage=$stage;contractPath=(Join-Path $root 'approved-contract.json');inventoryPath=$inventoryPath;contract=$contract}
}
function Case([string]$Name,[scriptblock]$Mutation,[bool]$Accept=$false) {
    $fixture = Fixture $Name
    & $Mutation $fixture
    $fixture.contract | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $fixture.contractPath -Encoding utf8NoBOM
    $accepted = $false
    $message = ''
    try { $report = & $validator -StageRoot $fixture.stage -ContractPath $fixture.contractPath -SourceInventoryPath $fixture.inventoryPath; if ($report.releaseReady -ne $false -or $report.reviewOnly -ne $true -or $report.unresolvedEvidence.Count -lt 1) { throw 'Missing review-only verdict' }; $accepted = $true } catch { $message = $_.Exception.Message }
    if ($accepted -ne $Accept) { throw "Case $Name failed: $message" }
    $script:results += @{case=$Name;passed=$true;accepted=$accepted;reason=$message}
}
Case 'complete-synthetic-stage' {} $true
Case 'unrelated-consistent-source-snapshot' {
    param($f)
    $f.contract.source.commit = ('f'*40)
    $inventory = Get-Content -LiteralPath $f.inventoryPath -Raw | ConvertFrom-Json
    $inventory.commit = $f.contract.source.commit
    $inventory | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $f.inventoryPath -Encoding utf8NoBOM
}
Case 'unrelated-consistent-source-tree' {
    param($f)
    $f.contract.source.tree = ('f'*40)
    $inventory = Get-Content -LiteralPath $f.inventoryPath -Raw | ConvertFrom-Json
    $inventory.tree = $f.contract.source.tree
    $inventory | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $f.inventoryPath -Encoding utf8NoBOM
}
Case 'extra-rom-shaped-file' { param($f) [IO.File]::WriteAllText((Join-Path $f.stage 'epr-synthetic.rom'),'synthetic sentinel') }
Case 'changed-runtime-bytes' { param($f) [IO.File]::AppendAllText((Join-Path $f.stage 'SDL2.dll'),'tampered') }
Case 'same-size-runtime-tamper' { param($f) $path = Join-Path $f.stage 'SDL2.dll'; $bytes = [IO.File]::ReadAllBytes($path); $bytes[0] = 88; [IO.File]::WriteAllBytes($path,$bytes) }
Case 'missing-runtime-file' { param($f) Remove-Item -LiteralPath (Join-Path $f.stage 'mpg123.dll') }
Case 'missing-runtime-resource' { param($f) $f.contract.files = @($f.contract.files | Where-Object path -NE 'res/tilemap.bin') }
Case 'resource-tamper-despite-updated-stage-hash' { param($f) $entry = $f.contract.files | Where-Object path -EQ 'res/tilemap.bin'; $path = Join-Path $f.stage $entry.path; $bytes = [IO.File]::ReadAllBytes($path); $bytes[0] = 88; [IO.File]::WriteAllBytes($path,$bytes); $entry.sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() }
Case 'missing-corresponding-source' { param($f) $f.contract.files = @($f.contract.files | Where-Object path -NE 'source/src/main/main.cpp') }
Case 'missing-dependency-source' { param($f) $f.contract.dependencies[0].source_paths = @() }
Case 'missing-notice' { param($f) $f.contract.files = @($f.contract.files | Where-Object path -NE 'notices/LGPL-2.1.txt') }
foreach ($notice in @('GPL-2.0.txt','dirent-LICENSE.txt','sdl2-LICENSE.txt','tinyxml2-LICENSE.txt','mpg123-LICENSE.txt','angle-LICENSE.txt','zlib-LICENSE.txt')) {
    $missingNotice = $notice
    Case ('missing-added-notice-'+$notice) { param($f) $f.contract.files = @($f.contract.files | Where-Object path -NE ('notices/'+$missingNotice)) }
}
Case 'duplicate-case-insensitive-path' { param($f) $f.contract.files += @{path='sdl2.DLL';role='runtime';bytes=1;sha256=('a'*64)} }
Case 'allowlisted-traversal' { param($f) $f.contract.files[0].path = '../outside.exe' }
Case 'allowlisted-personal-config' { param($f) $f.contract.files[0].path = 'config.xml' }
Case 'allowlisted-archive' { param($f) $f.contract.files[0].path = 'source.zip' }
Case 'wrong-game' { param($f) $f.contract.repository = 'd-b-c-e/OutRun2006' }
Case 'missing-build-identity' { param($f) $f.contract.build.compiler = '' }
Case 'wrong-source-git-identity' { param($f) $f.contract.source.inventory[0].blob = ('0'*40) }
Case 'source-tamper-despite-updated-stage-hash' { param($f) $entry = $f.contract.files | Where-Object path -EQ 'source/CMakeLists.txt'; $path = Join-Path $f.stage $entry.path; $bytes = [IO.File]::ReadAllBytes($path); $bytes[0] = 88; [IO.File]::WriteAllBytes($path,$bytes); $entry.sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant() }
Case 'shortened-source-inventory' { param($f) $f.contract.source.inventory = @($f.contract.source.inventory | Where-Object path -NE 'CMakeLists.txt') }
Case 'empty-private-directory' { param($f) New-Item -ItemType Directory -Path (Join-Path $f.stage 'roms') | Out-Null }
Case 'extra-hidden-state' { param($f) [IO.File]::WriteAllText((Join-Path $f.stage '.env'),'synthetic sentinel') }
Case 'reparse-point' { param($f) New-Item -ItemType Junction -Path (Join-Path $f.stage 'linked-source') -Target (Join-Path $f.stage 'source') | Out-Null }
$fixture = Fixture 'self-supplied-contract'
$fixture.contractPath = Join-Path $fixture.stage 'approved-contract.json'
$fixture.contract | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $fixture.contractPath -Encoding utf8NoBOM
try { $null = & $validator -StageRoot $fixture.stage -ContractPath $fixture.contractPath -SourceInventoryPath $fixture.inventoryPath; throw 'self-supplied contract unexpectedly passed' } catch { if ($_.Exception.Message -notlike '*contract must be external*') { throw } }
$results += @{case='self-supplied-contract';passed=$true;accepted=$false}
$fixture = Fixture 'evidence-record-gate'
$fixture.contract | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $fixture.contractPath -Encoding utf8NoBOM
$toolCopy = Join-Path $fixture.root 'isolated-tools'
New-Item -ItemType Directory -Path $toolCopy | Out-Null
Copy-Item -LiteralPath $validator -Destination $toolCopy
$copyValidator = Join-Path $toolCopy 'Test-ReleaseStage.ps1'
foreach ($mode in @('missing','cleared','release-claim','remove-source-blocker','resolved-nonblocking','replacement-clearance','unrelated-engine','duplicate-id','unknown-id','wrong-status','nonblocking','string-blocking','string-release-ready','case-changed-id')) {
    if ($mode -ne 'missing') {
        $record = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'unresolved-evidence.json') -Raw | ConvertFrom-Json
        switch ($mode) {
            'cleared' { $record.issues = @() }
            'release-claim' { $record.releaseReady = $true }
            'remove-source-blocker' { $record.issues = @($record.issues | Where-Object id -NE 'complete-dependency-sources') }
            'resolved-nonblocking' { foreach ($issue in $record.issues) { $issue.status = 'resolved'; $issue.blocksReleaseReview = $false } }
            'replacement-clearance' { $record.issues = @([pscustomobject]@{id='all-obligations-cleared';status='resolved';blocksReleaseReview=$false}) }
            'unrelated-engine' { $record.engineSnapshot = ('f'*40) }
            'duplicate-id' { $record.issues[1].id = $record.issues[0].id }
            'unknown-id' { $record.issues[1].id = 'all-obligations-cleared' }
            'wrong-status' { $record.issues[1].status = 'resolved' }
            'nonblocking' { $record.issues[1].blocksReleaseReview = $false }
            'string-blocking' { $record.issues[1].blocksReleaseReview = 'true' }
            'string-release-ready' { $record.releaseReady = 'false' }
            'case-changed-id' { $record.issues[1].id = 'COMPLETE-DEPENDENCY-SOURCES' }
        }
        $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $toolCopy 'unresolved-evidence.json') -Encoding utf8NoBOM
    }
    try {
        $null = & $copyValidator -StageRoot $fixture.stage -ContractPath $fixture.contractPath -SourceInventoryPath $fixture.inventoryPath
        throw 'evidence gate unexpectedly passed'
    } catch {
        if ($_.Exception.Message -notmatch 'required unresolved-evidence record is missing|invalid or prematurely cleared review-only evidence record') { throw }
    }
    $results += @{case=('evidence-record-'+$mode);passed=$true;accepted=$false}
}
$results | ConvertTo-Json -Depth 4
Write-Output "$($results.Count) synthetic cases passed; no runtime executed."
