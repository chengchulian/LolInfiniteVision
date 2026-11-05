[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Output 'Skipped: the assistant manifest requires administrator rights.'
    exit 77
}
$process = Start-Process -FilePath $Executable -ArgumentList '--smoke-test' -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(10000)) {
    $process.Kill()
    throw 'Assistant startup smoke test timed out.'
}
$process.Refresh()
exit $process.ExitCode
