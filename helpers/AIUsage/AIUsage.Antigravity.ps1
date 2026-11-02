function Add-AntigravityBucket($out,$key,$fraction,$reset) {
    if($null -eq $fraction) { return }
    $n=[double]$fraction
    if([double]::IsNaN($n) -or [double]::IsInfinity($n)) { return }
    $w=Window (100*$n) $reset
    if($w.resetEpoch -le 86400) { $w.resetEpoch=0 }
    $w.rawFraction=$n
    if(!$out.ContainsKey($key) -or $n -lt $out[$key].rawFraction) { $out[$key]=$w }
}
function Parse-Antigravity($response) {
    $out=@{}
    $groups=@()
    if($response.groups) { $groups=@($response.groups) }
    elseif($response.quotas) { $groups=@($response.quotas) }
    elseif($response.command -and $response.command.data -and $response.command.data.groups) { $groups=@($response.command.data.groups) }
    foreach($group in $groups) {
        $gname=[string]$group.displayName
        if(!$gname) { $gname=[string]$group.name }
        if(!$gname) { $gname=[string]$group.groupName }
        foreach($b in @($group.buckets)) {
            $bid=[string]$b.bucketId
            if(!$bid) { $bid=[string]$b.id }
            if(!$bid) { $bid=[string]$b.quotaId }
            $win=[string]$b.window
            $frac=$b.remainingFraction
            if($null -eq $frac) { $frac=$b.remaining_fraction }
            if($null -eq $frac -and $null -ne $b.remainingPercentage) { $frac=([double]$b.remainingPercentage)/100.0 }
            $rst=$b.resetTime
            if(!$rst) { $rst=$b.reset_time }
            if($bid -like '*argon*' -or $gname -match '(?i)argon') {
                Add-AntigravityBucket $out 'antigravityArgonWeek' $frac $rst
            } elseif($bid -like '3p-*' -or $bid -like 'claude-*' -or $gname -match '(?i)claude|gpt') {
                if($win -eq '5h' -or $bid -like '*-5h') { Add-AntigravityBucket $out 'antigravityClaude5h' $frac $rst }
                else { Add-AntigravityBucket $out 'antigravityClaudeWeek' $frac $rst }
            } elseif($bid -like 'gemini-*' -or $gname -match '(?i)gemini') {
                if($win -eq '5h' -or $bid -like '*-5h') { Add-AntigravityBucket $out 'antigravityGemini5h' $frac $rst }
                else { Add-AntigravityBucket $out 'antigravityGeminiWeek' $frac $rst }
            }
            Add-AntigravityBucket $out 'antigravityPlan' $frac $rst
        }
    }
    $models=@()
    if($out.Count -eq 0 -and $response.models) {
        foreach($prop in $response.models.PSObject.Properties) {
            $m=$prop.Value
            if($m.isInternal) { continue }
            if(!$m.displayName -and !$m.quotaInfo.resetTime) { continue }
            $models+=$m
        }
    }
    if($out.Count -eq 0 -and $models.Count -eq 0 -and $response.available_models.response.clientModelConfigs) {
        $models=@($response.available_models.response.clientModelConfigs)
    }
    if($out.Count -eq 0 -and $models.Count -eq 0 -and $response.user_status.userStatus.cascadeModelConfigData.clientModelConfigs) {
        $models=@($response.user_status.userStatus.cascadeModelConfigData.clientModelConfigs)
    }
    $labels=New-Object 'Collections.Generic.HashSet[string]'
    foreach($model in @($models)) {
        $quota=$model.quotaInfo
        if($null -eq $quota -or $null -eq $quota.remainingFraction) { continue }
        $label=$model.label
        if(!$label) { $label=$model.displayName }
        if($label -and !$labels.Add([string]$label)) { continue }
        $frac=$quota.remainingFraction
        $rst=$quota.resetTime
        if($label -match '(?i)argon') { Add-AntigravityBucket $out 'antigravityArgonWeek' $frac $rst }
        elseif($label -match '(?i)claude|gpt') { Add-AntigravityBucket $out 'antigravityClaude5h' $frac $rst }
        elseif($label -match '(?i)gemini') { Add-AntigravityBucket $out 'antigravityGemini5h' $frac $rst }
        Add-AntigravityBucket $out 'antigravityPlan' $frac $rst
    }
    if($out.Count -eq 0) { throw 'Antigravity model quota unavailable' }
    foreach($k in @($out.Keys)) { [void]$out[$k].Remove('rawFraction') }
    return $out
}
function Read-AntigravityCredential($target) {
    if(!('AntigravityCredReader' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class AntigravityCredReader {
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    struct CREDENTIAL {
        public uint Flags;
        public uint Type;
        public string TargetName;
        public string Comment;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
        public uint CredentialBlobSize;
        public IntPtr CredentialBlob;
        public uint Persist;
        public uint AttributeCount;
        public IntPtr Attributes;
        public string TargetAlias;
        public string UserName;
    }
    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    static extern bool CredRead(string target, uint type, int reservedFlag, out IntPtr credentialPtr);
    [DllImport("advapi32.dll", SetLastError = true)]
    static extern void CredFree([In] IntPtr cred);
    public static string Read(string target) {
        IntPtr ptr;
        if (!CredRead(target, 1, 0, out ptr)) return null;
        try {
            var cred = (CREDENTIAL)Marshal.PtrToStructure(ptr, typeof(CREDENTIAL));
            if (cred.CredentialBlob == IntPtr.Zero || cred.CredentialBlobSize == 0) return null;
            byte[] bytes = new byte[cred.CredentialBlobSize];
            Marshal.Copy(cred.CredentialBlob, bytes, 0, (int)cred.CredentialBlobSize);
            return Encoding.UTF8.GetString(bytes);
        } finally {
            CredFree(ptr);
        }
    }
}
'@
    }
    return [AntigravityCredReader]::Read($target)
}
function Refresh-AntigravityAccessToken($refreshToken) {
    if(!$refreshToken) { return $null }
    $agyExe=Join-Path $env:LOCALAPPDATA 'agy\bin\agy.exe'
    if(!(Test-Path -LiteralPath $agyExe)) {
        $cmd=Get-Command agy -ErrorAction SilentlyContinue
        if($cmd) { $agyExe=$cmd.Source }
    }
    if(!(Test-Path -LiteralPath $agyExe)) { return $null }
    $text=[System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($agyExe))
    $cid=[regex]::Match($text,'1071[0-9]+-[a-z0-9]+\.apps\.googleusercontent\.com').Value
    $csec=[regex]::Match($text,'GOCSPX-[A-Za-z0-9_-]{28}').Value
    if(!$cid -or !$csec) { return $null }
    $body=@{
        client_id=$cid
        client_secret=$csec
        grant_type='refresh_token'
        refresh_token=$refreshToken
    }
    $tok=Invoke-RestMethod -Uri 'https://oauth2.googleapis.com/token' -Method Post -Body $body -ContentType 'application/x-www-form-urlencoded' -TimeoutSec 15 -MaximumRedirection 0
    return $tok.access_token
}
function Invoke-AntigravityQuotaSummary($accessToken) {
    $headers=@{
        Authorization=('Bearer '+$accessToken)
        'Content-Type'='application/json'
        'User-Agent'='antigravity/cli/1.4.12 (os_type=windows; arch=amd64; aidev_client; cl=1010700967; auth_method=consumer)'
    }
    return Invoke-RestMethod -Uri 'https://daily-cloudcode-pa.googleapis.com/v1internal:retrieveUserQuotaSummary' -Method Post -Body '{}' -Headers $headers -TimeoutSec 15 -MaximumRedirection 0
}
function Invoke-AntigravityRpc($baseUri,$token,$method) {
    return Invoke-RestMethod -Uri ($baseUri+'/exa.language_server_pb.LanguageServerService/'+$method) -Method Post -Body '{}' -ContentType 'application/json' -Headers @{'Connect-Protocol-Version'='1';'x-codeium-csrf-token'=$token} -TimeoutSec 5 -MaximumRedirection 0
}
function Fetch-Antigravity($settings) {
    [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
    $raw=Read-AntigravityCredential 'gemini:antigravity'
    if($raw) {
        try {
            $parsed=$raw|ConvertFrom-Json
            $tokenObj=$parsed.token
            if(!$tokenObj) { $tokenObj=$parsed }
            $accessToken=$tokenObj.access_token
            if(!$accessToken) { $accessToken=$tokenObj.accessToken }
            $refreshToken=$tokenObj.refresh_token
            if(!$refreshToken) { $refreshToken=$tokenObj.refreshToken }
            if($tokenObj.expiry) {
                try {
                    if([DateTimeOffset]::Parse([string]$tokenObj.expiry,[cultureinfo]::InvariantCulture) -le [DateTimeOffset]::UtcNow.AddMinutes(1) -and $refreshToken) {
                        $refreshed=Refresh-AntigravityAccessToken $refreshToken
                        if($refreshed) { $accessToken=$refreshed }
                    }
                } catch { }
            }
            if($accessToken) {
                try {
                    $res=Invoke-AntigravityQuotaSummary $accessToken
                } catch {
                    $status=0
                    if($_.Exception.Response) { $status=[int]$_.Exception.Response.StatusCode }
                    if(($status -eq 401 -or $status -eq 403) -and $refreshToken) {
                        $accessToken=Refresh-AntigravityAccessToken $refreshToken
                        $res=Invoke-AntigravityQuotaSummary $accessToken
                    } else { throw }
                }
                return Parse-Antigravity $res
            }
        } catch {
            $status=0
            if($_.Exception.Response) { $status=[int]$_.Exception.Response.StatusCode }
            if($status -eq 401 -or $status -eq 403) { throw 'Antigravity login expired: open Antigravity (agy) to renew login' }
            throw 'Antigravity usage request failed: check network and login'
        }
    }
    $processes=@(Get-CimInstance Win32_Process -Filter "Name='language_server_windows_x64.exe'" -ErrorAction SilentlyContinue | Where-Object { $_.CommandLine -match '(?i)antigravity' })
    if($processes.Count -eq 0) { throw 'Sign in to Google Antigravity (agy) or start Antigravity IDE' }
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
