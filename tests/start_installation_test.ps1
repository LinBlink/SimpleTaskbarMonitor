$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\tools\start_installation.ps1')
$script:tasks=@();$script:startedTask=$null;$script:startedProcess=$null;$script:reject=$false
function Get-ScheduledTask {param($TaskPath,$ErrorAction) return $script:tasks}
function Start-ScheduledTask {param($TaskPath,$TaskName,$ErrorAction) if($script:reject){throw 'task access denied'};$script:startedTask=$TaskName}
function Start-Process {param($FilePath,$WorkingDirectory,$WindowStyle) $script:startedProcess=@($FilePath,$WorkingDirectory,$WindowStyle)}
function Task($path,$level='Highest',$arguments='') {
    return [pscustomobject]@{TaskPath='\TrafficMonitor\';TaskName=$path;State='Ready';Principal=[pscustomobject]@{RunLevel=$level};Actions=@([pscustomobject]@{Execute=$path;Arguments=$arguments})}
}
$binary='C:\_FLEX\TrafficMonitor\TrafficMonitor.exe'
$script:tasks=@((Task 'C:\other\TrafficMonitor.exe'),(Task $binary 'Limited'))
Start-TrafficMonitorInstallation -Executable $binary
if($script:startedTask -or $script:startedProcess[2] -ne 'Hidden') {throw 'Fallback must remain hidden and ignore other installations/limited tasks'}
$script:startedProcess=$null;$script:tasks=@((Task $binary))
Start-TrafficMonitorInstallation -Executable $binary
if($script:startedTask -ne $binary -or $script:startedProcess) {throw 'Must use matching elevated task'}
$script:startedTask=$null;$script:reject=$true
try {Start-TrafficMonitorInstallation -Executable $binary;throw 'missing failure'} catch {if($_.Exception.Message -ne 'task access denied'){throw}}
if($script:startedProcess) {throw 'Task rejection must not silently downgrade privileges'}
$script:reject=$false;$script:tasks=@((Task $binary 'Highest' '--other-action'))
Start-TrafficMonitorInstallation -Executable $binary
if($script:startedTask -or !$script:startedProcess) {throw 'Must not launch a task with extra arguments'}
Write-Host 'PASS: exact-path elevated task reuse, hidden fallback, no task creation, no silent privilege downgrade.'
