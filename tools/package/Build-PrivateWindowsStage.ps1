#Requires -Version 7.0
param(
    [Parameter(Mandatory)][ValidateSet('prepare','build','stage')][string]$Phase,
    [Parameter(Mandatory)][string]$RunRoot,
    [string]$DependencyRoot, [string]$DownloadCache,
    [string]$PythonExe = 'python', [string]$CMakeExe = 'cmake', [string]$PwshExe = 'pwsh'
)
$ErrorActionPreference = 'Stop'
$arguments = @((Join-Path $PSScriptRoot 'Private-WindowsStage.py'), $Phase, '--run-root', $RunRoot,
    '--cmake', $CMakeExe, '--pwsh', $PwshExe)
if ($DependencyRoot) { $arguments += @('--dependency-root', $DependencyRoot) }
if ($DownloadCache) { $arguments += @('--download-cache', $DownloadCache) }
& $PythonExe @arguments
if ($LASTEXITCODE) { throw "Private stage phase $Phase failed ($LASTEXITCODE); preserve its evidence and use a new run if needed." }
