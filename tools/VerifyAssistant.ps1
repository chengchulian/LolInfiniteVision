[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$Plugin
)
$ErrorActionPreference = 'Stop'
Import-Module Microsoft.PowerShell.Utility -ErrorAction Stop
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Output 'Skipped: the assistant manifest requires administrator rights.'
    exit 77
}
$root = Split-Path -Parent $PSScriptRoot
$build = (Resolve-Path -LiteralPath (Join-Path $root 'build')).Path
$fixtureName = 'assistant-fixture-' + (-join [char[]](0x4E2D, 0x6587)) + ' with spaces-' + [Guid]::NewGuid().ToString('N')
$fixture = Join-Path $build $fixtureName
$game = Join-Path $fixture 'Game'
[void][IO.Directory]::CreateDirectory($game)
[IO.File]::WriteAllText((Join-Path $game 'League of Legends.exe'), 'Fixture only; never executed')
$utf8 = [Text.UTF8Encoding]::new($false)
function Invoke-Assistant([string]$Operation = '--install', [string]$Loader = '') {
    $arguments = @($Operation, ('"{0}"' -f $fixture))
    if ($Loader) { $arguments += $Loader }
    $process = Start-Process -FilePath $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(10000)) {
        $process.Kill()
        throw 'Packaged assistant deployment timed out.'
    }
    $process.Refresh()
    return $process.ExitCode
}
function Assert-SameFile([string]$Actual, [string]$Expected) {
    if ((Get-FileHash -LiteralPath $Actual).Hash -ne (Get-FileHash -LiteralPath $Expected).Hash) {
        throw "Embedded payload mismatch: $Actual"
    }
}
try {
    if ((Invoke-Assistant) -ne 0) { throw 'Initial packaged deployment failed.' }
    if ((Test-Path -LiteralPath (Join-Path $game 'version.dll')) -or
        (Test-Path -LiteralPath (Join-Path $game 'dinput8.dll'))) { throw 'Plugin deployment also wrote a loader.' }
    Assert-SameFile (Join-Path $game 'load\LolInfiniteVision.dll') $Plugin
    $config = Join-Path $game 'load\LolInfiniteVision.ini'
    $text = [IO.File]::ReadAllText($config)
    foreach ($field in @('visionSwitch=1', 'attackRange=1', 'antiRecoil=1', 'towerRange=1', 'console=0')) {
        if ($text -notmatch ('(?m)^' + [regex]::Escape($field) + '\r?$')) { throw "Missing default setting: $field" }
    }
    foreach ($field in @('[Hotkeys]', 'visionSwitch=629', 'attackRange=630', 'antiRecoil=631', 'towerRange=632')) {
        if ($text -notmatch ('(?m)^' + [regex]::Escape($field) + '\r?$')) { throw "Missing default shortcut: $field" }
    }
    [IO.File]::WriteAllText($config, "[Config]`r`nvisionSwitch=0`r`nattackRange=0`r`nantiRecoil=0`r`ntowerRange=0`r`nconsole=1`r`ncustomSetting=909`r`n[Hotkeys]`r`nvisionSwitch=635`r`nattackRange=0`r`nantiRecoil=119`r`ntowerRange=120`r`n", $utf8)
    $other = Join-Path $game 'load\LolSurrender.dll'
    [IO.File]::WriteAllText($other, 'Preserve another plugin')
    if ((Invoke-Assistant) -ne 0) { throw 'Repeat packaged deployment failed.' }
    $text = [IO.File]::ReadAllText($config)
    foreach ($field in @('visionSwitch=0', 'attackRange=0', 'antiRecoil=0', 'towerRange=0', 'console=1', 'customSetting=909', 'visionSwitch=635', 'attackRange=0')) {
        if ($text -notmatch ('(?m)^' + [regex]::Escape($field) + '\r?$')) { throw "Existing setting was lost: $field" }
    }
    if ([IO.File]::ReadAllText($other) -ne 'Preserve another plugin') { throw 'Another plugin was modified.' }
    # Explicit loader deployment and cleanup leave the plugin and configuration unchanged.
    $pluginHash = (Get-FileHash -LiteralPath (Join-Path $game 'load\LolInfiniteVision.dll')).Hash
    $configHash = (Get-FileHash -LiteralPath $config).Hash
    if ((Invoke-Assistant '--install-loader' 'version') -ne 0) { throw 'Version loader deployment failed.' }
    Assert-SameFile (Join-Path $game 'version.dll') (Join-Path $root 'installer\assets\version.dll')
    if ((Invoke-Assistant '--install-loader' 'dinput8') -eq 0) { throw 'Installed a second project loader.' }
    # Shared-loader protection remains active.
    if ((Invoke-Assistant '--remove-loader' 'version') -eq 0) { throw 'Removed a loader shared by another plugin.' }
    Remove-Item -LiteralPath $other
    if ((Invoke-Assistant '--remove-loader' 'version') -ne 0) { throw 'Version loader cleanup failed.' }
    if (Test-Path -LiteralPath (Join-Path $game 'version.dll')) { throw 'Version loader cleanup left its target.' }
    if ((Invoke-Assistant '--install-loader' 'dinput8') -ne 0) { throw 'Dinput8 loader deployment failed.' }
    Assert-SameFile (Join-Path $game 'dinput8.dll') (Join-Path $root 'installer\assets\dinput8.dll')
    if ((Get-FileHash -LiteralPath $config).Hash -ne $configHash -or
        (Get-FileHash -LiteralPath (Join-Path $game 'load\LolInfiniteVision.dll')).Hash -ne $pluginHash) {
        throw 'Loader operations modified plugin files.'
    }
    [IO.File]::WriteAllText((Join-Path $game 'version.dll'), 'Foreign DLL')
    if ((Invoke-Assistant) -ne 0) { throw 'Plugin-only deployment was blocked by a foreign DLL.' }
    $snapshot = @{}
    Get-ChildItem -LiteralPath $game -Recurse -File | ForEach-Object { $snapshot[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash }
    if ((Invoke-Assistant '--install-loader' 'version') -eq 0) { throw 'Foreign DLL was overwritten.' }
    if ((Invoke-Assistant '--remove-loader' 'version') -eq 0) { throw 'Foreign DLL was removed.' }
    foreach ($path in $snapshot.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $snapshot[$path]) { throw "Loader conflict modified $path" }
    }
    Write-Output 'PASS: plugin-only deployment, both loader assets, independent cleanup, existing settings, shared and foreign DLL protection.'
} finally {
    $resolvedFixture = (Resolve-Path -LiteralPath $fixture).Path
    if ((Split-Path -Parent $resolvedFixture) -ne $build -or
        (Split-Path -Leaf $resolvedFixture) -notlike 'assistant-fixture-*') {
        throw 'Fixture cleanup escaped the build directory.'
    }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
