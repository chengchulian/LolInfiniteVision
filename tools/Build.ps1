[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Install Visual Studio or Build Tools with Desktop development with C++ (MSVC x64/x86, Windows SDK and CMake).'
}
$installations = @(& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json)
if ($installations.Count -eq 0) { throw 'No installation with the MSVC x64/x86 tools was found.' }
$installation = $installations[0]
$major = ([version]$installation.installationVersion).Major
switch ($major) {
    17 { $generator = 'Visual Studio 17 2022'; $buildName = 'vs2022-x64' }
    18 { $generator = 'Visual Studio 18 2026'; $buildName = 'vs2026-x64' }
    default { throw "Unsupported Visual Studio version: $major. Use CMake manually with an x64 MSVC toolchain." }
}
$cmake = Join-Path $installation.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) {
    $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if (-not $cmakeCommand) { throw 'CMake is missing. Add C++ CMake tools for Windows in Visual Studio Installer.' }
    $cmake = $cmakeCommand.Source
}
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
if (-not (Test-Path -LiteralPath $ctest)) { throw "CTest is missing beside $cmake" }
$build = Join-Path $root "build\$buildName"
& $cmake -S $root -B $build -G $generator -A x64 "-DCMAKE_GENERATOR_INSTANCE=$($installation.installationPath)" -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
& $ctest --test-dir $build -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed. No release package was created.' }
$distribution = Join-Path $root "dist\$Configuration"
[void][IO.Directory]::CreateDirectory($distribution)
Copy-Item -LiteralPath (Join-Path $build "$Configuration\LolInfiniteVision.dll") -Destination $distribution -Force
Copy-Item -LiteralPath (Join-Path $root 'LolInfiniteVision.ini') -Destination $distribution -Force
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $distribution -Force
$assistantName = (-join [char[]](0x65E0, 0x9650, 0x89C6, 0x8DDD, 0x52A9, 0x624B)) + '.exe'
Copy-Item -LiteralPath (Join-Path $build "$Configuration\LolInfiniteVisionAssistant.exe") -Destination (Join-Path $distribution $assistantName) -Force
Copy-Item -LiteralPath (Join-Path $distribution $assistantName) -Destination (Join-Path $root "dist\$assistantName") -Force
Write-Host "Build and tests passed. Output: $distribution"
