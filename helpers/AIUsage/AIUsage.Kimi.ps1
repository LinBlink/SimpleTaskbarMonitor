function ConvertTo-KimiWindow($key,$detail) {
    if ($null -eq $detail) { return $null }
    $remaining=$null
    foreach($name in @('used_ratio','usedRatio')) {
        if($null -ne $detail.$name) { $remaining=100*(1-[double]$detail.$name); break }
    }
    $limit=$detail.limit
    if($null -eq $remaining -and $null -ne $limit -and [double]$limit -gt 0) {
        if($null -ne $detail.remaining) { $remaining=100*[double]$detail.remaining/[double]$limit }
        elseif($null -ne $detail.used) { $remaining=100*(1-[double]$detail.used/[double]$limit) }
    }
    if($null -eq $remaining) { return $null }
    $reset=$detail.resetTime
    if($null -eq $reset) { $reset=$detail.reset_time }
    return @{key=$key;window=(Window $remaining $reset)}
}
function Parse-Kimi($response) {
    $out=@{}
    if($response.usages -is [array]) {
        $entry=$response.usages[0]
        if($null -eq $entry) { throw 'Kimi subscription quota unavailable' }
        $converted=ConvertTo-KimiWindow 'kimiWeek' $entry.detail
        if($converted) { $out[$converted.key]=$converted.window }
        foreach($limit in @($entry.limits)) {
            $converted=ConvertTo-KimiWindow 'kimi5h' $limit.detail
            if($converted) { $out[$converted.key]=$converted.window; break }
        }
    } else {
        $usages=$response.usages
        if($usages) {
            foreach($spec in @(@('kimi5h','limit_5h'),@('kimiWeek','limit_7d'),@('kimiWeek','limit_month_total'),@('kimiWeek','limit_month_code'))) {
                if($out.ContainsKey($spec[0])) { continue }
                $converted=ConvertTo-KimiWindow $spec[0] $usages.($spec[1])
                if($converted) { $out[$converted.key]=$converted.window }
            }
        }
        $converted=ConvertTo-KimiWindow 'kimiWeek' $response.usage
        if($converted -and !$out.ContainsKey('kimiWeek')) { $out[$converted.key]=$converted.window }
        $limits=@($response.limits)
        $selected=$limits | Where-Object { $_.duration -eq 300 -or $_.detail.duration -eq 300 } | Select-Object -First 1
        if($null -eq $selected -and $limits.Count -eq 1) { $selected=$limits[0] }
        if($selected) {
            $converted=ConvertTo-KimiWindow 'kimi5h' $selected.detail
            if($converted -and !$out.ContainsKey('kimi5h')) { $out[$converted.key]=$converted.window }
        }
    }
    if($out.Count -eq 0) { throw 'Unsupported Kimi quota response' }
    return $out
}
function Fetch-Kimi($settings) {
    $file=$settings.KimiCredentials
    if(!$file) {
        $root=$env:KIMI_CODE_HOME
        if(!$root) { $root=Join-Path $env:USERPROFILE '.kimi-code' }
        $file=Join-Path $root 'credentials/kimi-code.json'
    }
    if(!(Test-Path -LiteralPath $file -PathType Leaf)) { throw 'Sign in to Kimi Code first' }
    try { $auth=[IO.File]::ReadAllText($file)|ConvertFrom-Json }
    catch { throw 'Cannot read Kimi Code login' }
    if(!$auth.access_token) { throw 'Kimi Code login not found' }
    if($auth.expires_at) {
        try {
            $expires=[double]$auth.expires_at
            if($expires -lt 100000000000) { $expires*=1000 }
            if($expires -le [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()) { throw 'Kimi login expired: open Kimi Code to renew your login' }
        } catch [System.Management.Automation.RuntimeException] {
            if($_.Exception.Message -like 'Kimi login expired:*') { throw }
        }
    }
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    try {
        $response=Invoke-RestMethod -Uri 'https://api.kimi.com/coding/v1/usages' -Headers @{Authorization=('Bearer '+$auth.access_token);Accept='application/json'} -TimeoutSec 20 -MaximumRedirection 0
    } catch {
        $status=0
        if($_.Exception.Response) { $status=[int]$_.Exception.Response.StatusCode }
        if($status -eq 401 -or $status -eq 403) { throw 'Kimi login rejected: open Kimi Code and sign in again' }
        throw 'Kimi usage request failed: check network and subscription login'
    } finally { $auth=$null }
    return Parse-Kimi $response
}
