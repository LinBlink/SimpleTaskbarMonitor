function Parse-Claude($response) {
    $out=@{}
    foreach($spec in @(@('claude5h','five_hour'),@('claudeWeek','seven_day'))) {
        $quota=$response.($spec[1])
        if($null -eq $quota -or $null -eq $quota.utilization) { continue }
        $reset=0
        if($quota.resets_at) {
            try { $reset=[DateTimeOffset]::Parse($quota.resets_at,[cultureinfo]::InvariantCulture).ToUnixTimeSeconds() }
            catch { throw 'Invalid Claude reset time' }
        }
        $out[$spec[0]]=Window (100-[double]$quota.utilization) $reset
    }
    if($out.Count -eq 0) { throw 'Claude subscription quota unavailable' }
    return $out
}
function Fetch-Claude($settings) {
    $file=$settings.ClaudeCredentials
    if(!$file) {
        $root=$env:CLAUDE_CONFIG_DIR
        if(!$root) { $root=Join-Path $env:USERPROFILE '.claude' }
        $file=Join-Path $root '.credentials.json'
    }
    if(!(Test-Path -LiteralPath $file -PathType Leaf)) { throw 'Sign in to Claude Code with your subscription first' }
    try { $auth=([IO.File]::ReadAllText($file)|ConvertFrom-Json).claudeAiOauth }
    catch { throw 'Cannot read Claude Code login' }
    if(!$auth.accessToken) { throw 'Claude subscription login not found (API keys are not supported)' }
    if($auth.expiresAt -and [double]$auth.expiresAt -le ([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())) {
        throw 'Claude login expired: open Claude Code to renew your login'
    }
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    try {
        $response=Invoke-RestMethod -Uri 'https://api.anthropic.com/api/oauth/usage' -Headers @{Authorization=('Bearer '+$auth.accessToken);'anthropic-beta'='oauth-2025-04-20';Accept='application/json'} -TimeoutSec 15 -MaximumRedirection 0
    } catch {
        $status=0
        if($_.Exception.Response) { $status=[int]$_.Exception.Response.StatusCode }
        if($status -eq 429) { throw 'Claude rate limited: retry in 30 minutes' }
        if($status -eq 401 -or $status -eq 403) { throw 'Claude login rejected: open Claude Code and sign in again' }
        throw 'Claude usage request failed: check network and subscription login'
    } finally { $auth=$null }
    return Parse-Claude $response
}
