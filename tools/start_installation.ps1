# Reuse the user's existing elevated autorun task; never create or modify tasks.
function Start-SimpleTaskbarInstallation {
    param([Parameter(Mandatory=$true)][string]$Executable)
    $binary=[IO.Path]::GetFullPath($Executable)
    $tasks=@(Get-ScheduledTask -TaskPath '\TrafficMonitor\' -ErrorAction SilentlyContinue | Sort-Object TaskName)
    foreach($task in $tasks) {
        if($task.State -eq 'Disabled' -or $task.Principal.RunLevel -ne 'Highest') {continue}
        $actions=@($task.Actions)
        if($actions.Count -ne 1 -or ![string]::IsNullOrWhiteSpace($actions[0].Arguments)) {continue}
        try {$actionPath=[IO.Path]::GetFullPath([Environment]::ExpandEnvironmentVariables($actions[0].Execute.Trim('"')))} catch {continue}
        if(![string]::Equals($actionPath,$binary,[StringComparison]::OrdinalIgnoreCase)) {continue}
        # If this fails, propagate the error instead of silently losing CPU driver access.
        Start-ScheduledTask -TaskPath $task.TaskPath -TaskName $task.TaskName -ErrorAction Stop
        Write-Host "Started through existing elevated task: $($task.TaskPath)$($task.TaskName)"
        return
    }
    Start-Process -FilePath $binary -WorkingDirectory ([IO.Path]::GetDirectoryName($binary)) -WindowStyle Hidden
}
