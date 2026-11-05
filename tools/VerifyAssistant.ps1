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
function Invoke-Assistant {
    $process = Start-Process -FilePath $Executable -ArgumentList @('--install', ('"{0}"' -f $fixture)) -WindowStyle Hidden -PassThru
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
    Assert-SameFile (Join-Path $game 'version.dll') (Join-Path $root 'installer\assets\version.dll')
    Assert-SameFile (Join-Path $game 'load\LolInfiniteVision.dll') $Plugin
    $config = Join-Path $game 'load\LolInfiniteVision.ini'
    $text = [IO.File]::ReadAllText($config)
    foreach ($field in @('visionSwitch=1', 'attackRange=1', 'antiRecoil=1', 'towerRange=1', 'console=0')) {
        if ($text -notmatch ('(?m)^' + [regex]::Escape($field) + '\r?$')) { throw "Missing default setting: $field" }
    }
    [IO.File]::WriteAllText($config, "[Config]`r`nvisionSwitch=0`r`nattackRange=0`r`nantiRecoil=0`r`ntowerRange=0`r`nconsole=1`r`ncustomSetting=909`r`n", $utf8)
    $other = Join-Path $game 'load\LolSurrender.dll'
    [IO.File]::WriteAllText($other, 'Preserve another plugin')
    if ((Invoke-Assistant) -ne 0) { throw 'Repeat packaged deployment failed.' }
    $text = [IO.File]::ReadAllText($config)
    foreach ($field in @('visionSwitch=0', 'attackRange=0', 'antiRecoil=0', 'towerRange=0', 'console=1', 'customSetting=909')) {
        if ($text -notmatch ('(?m)^' + [regex]::Escape($field) + '\r?$')) { throw "Existing setting was lost: $field" }
    }
    if ([IO.File]::ReadAllText($other) -ne 'Preserve another plugin') { throw 'Another plugin was modified.' }
    [IO.File]::WriteAllText((Join-Path $game 'version.dll'), 'Conflicting loader')
    $snapshot = @{}
    Get-ChildItem -LiteralPath $game -Recurse -File | ForEach-Object { $snapshot[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName).Hash }
    if ((Invoke-Assistant) -eq 0) { throw 'Conflicting loader was accepted.' }
    foreach ($path in $snapshot.Keys) {
        if ((Get-FileHash -LiteralPath $path).Hash -ne $snapshot[$path]) { throw "Loader conflict modified $path" }
    }
    Write-Output 'PASS: packaged assets, Chinese path with spaces, existing settings, plugin coexistence and loader conflict.'
} finally {
    $resolvedFixture = (Resolve-Path -LiteralPath $fixture).Path
    if ((Split-Path -Parent $resolvedFixture) -ne $build -or
        (Split-Path -Leaf $resolvedFixture) -notlike 'assistant-fixture-*') {
        throw 'Fixture cleanup escaped the build directory.'
    }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
