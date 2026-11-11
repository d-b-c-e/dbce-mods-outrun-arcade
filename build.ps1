# Builds the game and runs every offline test. No arguments needed: `pwsh build.ps1`.
# No game is launched (input_inject_test drives the real input handlers with an SDL virtual joystick) and nothing is installed.
# The original OutRun ROMs are not needed to compile or test, so everything is built. First run installs the vcpkg packages (network).
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
# cmd truncates a PATH over 8191 characters, and vcvars then loses the Windows SDK. Keep only what the build needs.
if ($env:PATH.Length -gt 8000) {
    $keep = 'git', 'cmake', 'ninja', 'python', 'pwsh' | ForEach-Object { (Get-Command $_ -ErrorAction SilentlyContinue | Select-Object -First 1).Source } | Where-Object { $_ } | ForEach-Object { Split-Path $_ }
    $env:PATH = (@("$env:WINDIR\System32", $env:WINDIR, "$env:WINDIR\System32\WindowsPowerShell\v1.0", "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer") + $keep | Select-Object -Unique) -join ';'
}
$vcpkg = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg' }
$toolchain = (Join-Path $vcpkg 'scripts\buildsystems\vcpkg.cmake').Replace([string][char]92, '/')
if (-not (Test-Path -LiteralPath $toolchain)) { throw "No vcpkg toolchain at $toolchain. Set VCPKG_ROOT." }

# 1. The standalone C++/Python test project (ctest).
$tests = 'obj\offline-tests'
cmake -S tests -B $tests
if ($LASTEXITCODE) { throw 'tests configure failed' }
cmake --build $tests --config Release
if ($LASTEXITCODE) { throw 'tests build failed' }
ctest --test-dir $tests -C Release --output-on-failure
if ($LASTEXITCODE) { throw 'ctest failed' }

# 2. The game itself, plus input_inject_test (part of the main project).
$game = 'obj\game-build'
cmake -S . -B $game -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
if ($LASTEXITCODE) { throw 'game configure failed' }
cmake --build $game --config Release
if ($LASTEXITCODE) { throw 'game build failed' }
& "$game\Release\input_inject_test.exe"
if ($LASTEXITCODE) { throw 'input_inject_test failed' }
'build and offline tests passed'
