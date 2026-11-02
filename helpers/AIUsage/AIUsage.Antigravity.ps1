function Parse-Antigravity($response) {
    $models=$response.available_models.response.clientModelConfigs
    if(!$models) { $models=$response.user_status.userStatus.cascadeModelConfigData.clientModelConfigs }
    $best=$null
    $labels=New-Object 'Collections.Generic.HashSet[string]'
    foreach($model in @($models)) {
        $quota=$model.quotaInfo
        if($null -eq $quota -or $null -eq $quota.remainingFraction) { continue }
        $label=$model.label
        if(!$label) { $label=$model.displayName }
        if($label -and !$labels.Add([string]$label)) { continue }
        $fraction=[double]$quota.remainingFraction
        if([double]::IsNaN($fraction) -or [double]::IsInfinity($fraction)) { continue }
        if($null -eq $best -or $fraction -lt $best.fraction) { $best=@{fraction=$fraction;reset=$quota.resetTime} }
    }
    if($null -eq $best) { throw 'Antigravity model quota unavailable' }
    return @{antigravityPlan=(Window (100*$best.fraction) $best.reset)}
}
function Invoke-AntigravityRpc($baseUri,$token,$method) {
    return Invoke-RestMethod -Uri ($baseUri+'/exa.language_server_pb.LanguageServerService/'+$method) -Method Post -Body '{}' -ContentType 'application/json' -Headers @{'Connect-Protocol-Version'='1';'x-codeium-csrf-token'=$token} -TimeoutSec 5 -MaximumRedirection 0
}
function Fetch-Antigravity($settings) {
    $processes=@(Get-CimInstance Win32_Process -Filter "Name='language_server_windows_x64.exe'" -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -match '(?i)antigravity' })
    if($processes.Count -eq 0) { throw 'Start Google Antigravity before querying quota' }
    $oldCallback=[Net.ServicePointManager]::ServerCertificateValidationCallback
    try {
        [Net.ServicePointManager]::ServerCertificateValidationCallback={ $true }
        foreach($process in $processes) {
            $match=[regex]::Match($process.CommandLine,'(?:^|\s)--csrf_token(?:=|\s+)"?([^\s"]+)')
            if(!$match.Success) { continue }
            $token=$match.Groups[1].Value
            $ports=@(Get-NetTCPConnection -State Listen -OwningProcess $process.ProcessId -ErrorAction SilentlyContinue | Select-Object -ExpandProperty LocalPort -Unique)
            foreach($port in $ports) {
                foreach($scheme in @('https','http')) {
                    try {
                        $baseUri=$scheme+'://127.0.0.1:'+$port
                        $status=Invoke-AntigravityRpc $baseUri $token 'GetUserStatus'
                        $models=Invoke-AntigravityRpc $baseUri $token 'GetAvailableModels'
                        return Parse-Antigravity @{user_status=$status;available_models=$models}
                    } catch { }
                }
            }
        }
    } finally {
        [Net.ServicePointManager]::ServerCertificateValidationCallback=$oldCallback
    }
    throw 'Antigravity local usage endpoint is unavailable'
}
