$ErrorActionPreference = 'Stop'
Write-Host 'MiniMax subscription key setup (encrypted for this Windows user).'
Write-Host 'Leave empty to keep the saved key. Never paste the key into chat.'
$key = Read-Host 'MiniMax Subscription Key' -AsSecureString
if ($key.Length -gt 0) {
    $encrypted = ConvertFrom-SecureString $key
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'AIUsage.minimax.key'), $encrypted)
    Write-Host 'Saved encrypted key.'
}
Start-Process notepad.exe -ArgumentList ('"' + (Join-Path $PSScriptRoot 'AIUsage.ini') + '"')
Write-Host 'Set MiniMaxRegion=cn or global in AIUsage.ini. Codex uses existing ChatGPT login.'
