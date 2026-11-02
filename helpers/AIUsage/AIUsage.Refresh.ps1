param([switch]$LibraryOnly)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/AIUsage.Cursor.ps1"
. "$PSScriptRoot/AIUsage.Claude.ps1"
. "$PSScriptRoot/AIUsage.Kimi.ps1"
. "$PSScriptRoot/AIUsage.Antigravity.ps1"
function Read-Settings {
    $settings = @{}
    $path = Join-Path $PSScriptRoot 'AIUsage.ini'
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        foreach ($line in [IO.File]::ReadAllLines($path)) {
            if ($line -match '^\s*([^;#\[=]+?)\s*=\s*(.*?)\s*$') { $settings[$matches[1]] = $matches[2] }
        }
    }
    return $settings
}
function Percent($remaining) {
    $n = [double]$remaining
    if ([double]::IsNaN($n) -or [double]::IsInfinity($n)) { throw 'Invalid percentage' }
    return ([math]::Round([math]::Max(0,[math]::Min(100,$n)),0).ToString('0',[cultureinfo]::InvariantCulture) + '%')
}
function Reset-Epoch($value) {
    if ($null -eq $value -or "$value" -eq '') { return 0 }
    try {
        $n=[double]$value
        if($n -gt 100000000000) { $n/=1000 }
        return [long]$n
    } catch {
        try { return [DateTimeOffset]::Parse([string]$value,[cultureinfo]::InvariantCulture).ToUnixTimeSeconds() }
        catch { return 0 }
    }
}
function Reset-Time($value) {
    $epoch=Reset-Epoch $value
    if($epoch -le 0) { return '' }
    return [DateTimeOffset]::FromUnixTimeSeconds($epoch).LocalDateTime.ToString('MM-dd HH:mm')
}
function Window($percent,$reset) {
    $resetEpoch=Reset-Epoch $reset
    return @{value=(Percent $percent);reset=(Reset-Time $reset);resetEpoch=$resetEpoch;status='OK';updated=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()}
}
function Parse-Codex($result) {
    $bucket = $result.rateLimits
    if ($null -ne $result.rateLimitsByLimitId.codex) { $bucket = $result.rateLimitsByLimitId.codex }
    if ($null -eq $bucket) { throw 'No rate limit snapshot' }
    $out = @{}
    foreach ($w in @($bucket.primary,$bucket.secondary)) {
        if ($null -eq $w -or $null -eq $w.usedPercent) { continue }
        # Select by actual duration, not by primary/secondary position.
        if ($w.windowDurationMins -eq 300) { $out.codex5h = Window (100-[double]$w.usedPercent) $w.resetsAt }
        if ($w.windowDurationMins -eq 10080) { $out.codexWeek = Window (100-[double]$w.usedPercent) $w.resetsAt }
    }
    return $out
}
function Parse-MiniMax($response,$settings) {
    if ($null -ne $response.base_resp -and $response.base_resp.status_code -ne 0) { throw 'MiniMax authentication or service error' }
    if ($null -ne $response.code -and $response.code -ne 0) { throw 'MiniMax service error' }
    $body = $response
    if ($null -ne $response.data) { $body = $response.data }
    $rows = @($body.model_remains)
    if ($null -ne $body.model_remains) {
        $row = $rows | Where-Object { $_.model_name -eq $settings.MiniMaxModel } | Select-Object -First 1
        if ($null -eq $row -and $rows.Count -eq 1) { $row = $rows[0] }
        if ($null -eq $row) { throw 'MiniMax model not found: configure MiniMaxModel' }
    } else { $row = $body }
    $out = @{}
    foreach ($spec in @(
        @('minimax5h','current_interval_total_count','current_interval_usage_count','current_interval_remaining_count','end_time','current_interval_remaining_percent'),
        @('minimaxWeek','current_weekly_total_count','current_weekly_usage_count','current_weekly_remaining_count','weekly_end_time','current_weekly_remaining_percent')
    )) {
        # New Token Plan responses provide authoritative percentages while legacy counts are zero.
        $percent = $row.($spec[5])
        if ($null -ne $percent) {
            $out[$spec[0]] = Window $percent $row.($spec[4])
            continue
        }
        $total = $row.($spec[1]); $count = $row.($spec[2]); $left = $row.($spec[3])
        if ($null -eq $total -or [double]$total -le 0) { continue }
        if ($null -eq $left) {
            if ($null -eq $count) { continue }
            if ($settings.MiniMaxCountMeaning -eq 'used') { $left = [double]$total-[double]$count }
            elseif ($settings.MiniMaxCountMeaning -eq 'remaining') { $left = $count }
            else { throw 'MiniMaxCountMeaning must be remaining or used' }
        }
        $out[$spec[0]] = Window (100*[double]$left/[double]$total) $row.($spec[4])
    }
    if ($out.Count -eq 0) { throw 'Unsupported MiniMax quota response' }
    return $out
}
function Find-Codex($settings) {
    if ($settings.CodexExe) {
        if (Test-Path -LiteralPath $settings.CodexExe -PathType Leaf) { return $settings.CodexExe }
        throw 'CodexExe does not exist'
    }
    $root = $env:CODEX_HOME
    if (!$root) { $root = Join-Path $env:USERPROFILE '.codex' }
    $releases = Join-Path $root 'packages/standalone/releases'
    if (Test-Path -LiteralPath $releases) {
        $found = Get-ChildItem -LiteralPath $releases -Directory | Sort-Object {
            try { [version](($_.Name -split '-')[0]) } catch { [version]'0.0' }
        } -Descending | ForEach-Object { Join-Path $_.FullName 'bin/codex.exe' } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if ($found) { return $found }
    }
    foreach ($path in @((Join-Path $env:LOCALAPPDATA 'Programs/OpenAI/Codex/bin/codex.exe'))) {
        if (Test-Path -LiteralPath $path) { return $path }
    }
    $command = Get-Command codex.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    throw 'Codex executable not found: configure CodexExe'
}
function Read-Rpc($proc,$id) {
    $deadline = [DateTime]::UtcNow.AddSeconds(25)
    while ([DateTime]::UtcNow -lt $deadline) {
        $task = $proc.StandardOutput.ReadLineAsync()
        $remaining = [math]::Max(1,[int]($deadline-[DateTime]::UtcNow).TotalMilliseconds)
        if (!$task.Wait($remaining)) { throw 'Codex query timed out' }
        $line = $task.Result
        if ($null -eq $line) { throw 'Codex app-server exited' }
        try { $msg = $line | ConvertFrom-Json } catch { continue }
        if ($null -ne $msg.id -and $msg.id -eq $id) {
            if ($null -ne $msg.error) { throw 'Codex query failed: sign in with ChatGPT or check network' }
            return $msg.result
        }
    }
    throw 'Codex query timed out'
}
function Fetch-Codex($settings) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = Find-Codex $settings
    $info.Arguments = 'app-server'
    $info.UseShellExecute = $false; $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $true; $info.RedirectStandardOutput = $true; $info.RedirectStandardError = $true
    $proc = New-Object Diagnostics.Process
    $proc.StartInfo = $info
    try {
        [void]$proc.Start()
        # Drain stderr without persisting messages that could contain account details.
        $stderr = $proc.StandardError.ReadToEndAsync()
        $proc.StandardInput.WriteLine('{"id":1,"method":"initialize","params":{"clientInfo":{"name":"trafficmonitor_usage","version":"1.0.0"}}}')
        $null = Read-Rpc $proc 1
        $proc.StandardInput.WriteLine('{"method":"initialized","params":{}}')
        $proc.StandardInput.WriteLine('{"id":2,"method":"account/rateLimits/read"}')
        return Parse-Codex (Read-Rpc $proc 2)
    } finally {
        if ($proc.Id) {
            $proc.StandardInput.Close()
            if (!$proc.WaitForExit(1500)) { $proc.Kill(); [void]$proc.WaitForExit(2000) }
        }
        $proc.Dispose()
    }
}
function Fetch-MiniMax($settings) {
    $key = $env:MINIMAX_API_KEY
    $keyFile = Join-Path $PSScriptRoot 'AIUsage.minimax.key'
    if (Test-Path -LiteralPath $keyFile) {
        $secure = [IO.File]::ReadAllText($keyFile) | ConvertTo-SecureString
        $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
        try { $key = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr) }
        finally { [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr) }
    }
    if (!$key) { throw 'Configure MiniMax subscription key in plugin options' }
    $endpoint = switch ($settings.MiniMaxRegion) {
        'cn' { 'https://www.minimax.cn/v1/token_plan/remains' }
        'global' { 'https://www.minimax.io/v1/token_plan/remains' }
        default { throw 'MiniMaxRegion must be cn or global' }
    }
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    try {
        $data = Invoke-RestMethod -Uri $endpoint -Headers @{Authorization="Bearer $key"} -ContentType 'application/json' -TimeoutSec 20 -MaximumRedirection 0
    } catch { throw 'MiniMax request failed: check key, region and network' }
    finally { $key = $null }
    return Parse-MiniMax $data $settings
}
function Read-Cache($path) {
    $cache = @{}; $section = ''
    if (Test-Path -LiteralPath $path) {
        foreach ($line in [IO.File]::ReadAllLines($path)) {
            if ($line -match '^\[(.+)\]$') { $section=$matches[1];$cache[$section]=@{} }
            elseif ($section -and $line -match '^([^=]+)=(.*)$') { $cache[$section][$matches[1]]=$matches[2] }
        }
    }
    return $cache
}
function Refresh-Usage {
    $settings = Read-Settings
    $path = Join-Path $PSScriptRoot 'AIUsage.cache.ini'
    $cache = Read-Cache $path
    foreach ($provider in @('codex','minimax','cursor','claude','kimi','antigravity')) {
        $keys = switch($provider) {
            'codex' { @('codex5h','codexWeek') }
            'minimax' { @('minimax5h','minimaxWeek') }
            'cursor' { @('cursorPlan') }
            'claude' { @('claude5h','claudeWeek') }
            'kimi' { @('kimi5h','kimiWeek') }
            'antigravity' { @('antigravityPlan') }
        }
        $setting='Enable'+[cultureinfo]::InvariantCulture.TextInfo.ToTitleCase($provider)
        if($settings.ContainsKey($setting) -and $settings[$setting] -eq '0') { continue }
        $now=[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
        # An expired credential is checked locally; recheck each cycle so a renewed login
        # recovers immediately without waiting for the previous authentication cooldown.
        if($provider -eq 'claude' -and $cache.ContainsKey('claude5h') -and $cache.claude5h.status -notlike 'Claude login expired:*' -and $cache.claude5h.nextAttempt -and [long]$cache.claude5h.nextAttempt -gt $now) { continue }
        try {
            $fetched = switch($provider) {
                'codex' { Fetch-Codex $settings }
                'minimax' { Fetch-MiniMax $settings }
                'cursor' { Fetch-Cursor $settings }
                'claude' { Fetch-Claude $settings }
                'kimi' { Fetch-Kimi $settings }
                'antigravity' { Fetch-Antigravity $settings }
            }
            foreach ($k in $keys) {
                if ($fetched.ContainsKey($k)) { $cache[$k]=$fetched[$k] }
                else { $cache[$k]=@{value='--';reset='';updated=0;status='Quota window unavailable'} }
                if($provider -eq 'claude') { $cache[$k].nextAttempt=$now+600 }
            }
        } catch {
            # All provider errors are generated locally; no raw upstream body is logged.
            $message = $_.Exception.Message -replace '[\r\n]',' '
            foreach ($k in $keys) {
                if (!$cache.ContainsKey($k)) { $cache[$k]=@{value='--';reset='';updated=0} }
                $cache[$k].status=$message
                if($provider -eq 'claude') { $cache[$k].nextAttempt=$now+$(if($message -like '*rate limited*') {1800} elseif($message -like '*request failed*') {120} else {600}) }
            }
        }
    }
    $lines = New-Object 'Collections.Generic.List[string]'
    foreach ($k in @('minimax5h','minimaxWeek','codex5h','codexWeek','cursorPlan','claude5h','claudeWeek','kimi5h','kimiWeek','antigravityPlan')) {
        $lines.Add("[$k]")
        foreach ($field in @('value','reset','resetEpoch','updated','status','nextAttempt')) { $lines.Add("$field=$($cache[$k][$field])") }
    }
    $temp = $path+'.'+$PID+'.tmp'
    [IO.File]::WriteAllLines($temp,$lines,[Text.Encoding]::Unicode)
    if (Test-Path -LiteralPath $path) { [IO.File]::Replace($temp,$path,($path+'.bak')); Remove-Item -LiteralPath ($path+'.bak') }
    else { [IO.File]::Move($temp,$path) }
}
if (!$LibraryOnly) { Refresh-Usage }
