function Parse-Antigravity($response) {
    $models=@()
    if($response.models) {
        foreach($prop in $response.models.PSObject.Properties) {
            $m=$prop.Value
            if($m.isInternal) { continue }
            if(!$m.displayName -and !$m.quotaInfo.resetTime) { continue }
            $models+=$m
        }
    }
    if($models.Count -eq 0 -and $response.available_models.response.clientModelConfigs) {
        $models=@($response.available_models.response.clientModelConfigs)
    }
    if($models.Count -eq 0 -and $response.user_status.userStatus.cascadeModelConfigData.clientModelConfigs) {
        $models=@($response.user_status.userStatus.cascadeModelConfigData.clientModelConfigs)
    }
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
            if($accessToken) {
                $res=Invoke-RestMethod -Uri 'https://daily-cloudcode-pa.googleapis.com/v1internal:fetchAvailableModels' -Method Post -Body '{}' -Headers @{Authorization=('Bearer '+$accessToken);'Content-Type'='application/json';'User-Agent'='antigravity/1.4.12'} -TimeoutSec 15 -MaximumRedirection 0
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
