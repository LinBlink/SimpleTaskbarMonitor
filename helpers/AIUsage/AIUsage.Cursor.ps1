function Parse-Cursor($response) {
    $plan=$response.individualUsage.plan
    if($null -eq $plan) { $plan=$response.planUsage }
    if($null -eq $plan -and $response.usage) { $plan=$response.usage.planUsage }
    if($null -eq $plan -or $plan.enabled -eq $false) { throw 'Cursor included plan quota unavailable' }
    $reset=0
    $rawEnd=$response.billingCycleEnd
    if(!$rawEnd -and $response.usage) { $rawEnd=$response.usage.billingCycleEnd }
    if($rawEnd) {
        $reset=Reset-Epoch $rawEnd
        if($reset -le 0) { throw 'Invalid Cursor billing reset time' }
    }
    $out=@{}
    if($null -ne $plan.totalPercentUsed) { $out.cursorPlan=Window (100-[double]$plan.totalPercentUsed) $reset }
    # Old accounts may expose a numeric included budget without a percentage.
    if(!$out.ContainsKey('cursorPlan') -and $null -ne $plan.limit -and [double]$plan.limit -gt 0) {
        $remaining=$plan.remaining
        $used=$plan.used
        if($null -eq $used) { $used=$plan.includedSpend }
        if($null -eq $remaining -and $null -ne $used) { $remaining=[double]$plan.limit-[double]$used }
        if($null -ne $remaining) { $out.cursorPlan=Window (100*[double]$remaining/[double]$plan.limit) $reset }
    }
    if($out.Count -eq 0) { throw 'Unsupported Cursor included quota response' }
    return $out
}
function Fetch-Cursor($settings) { return Parse-Cursor (Fetch-CursorResponse $settings) }
function Read-CursorCliToken {
    foreach($path in @(
        (Join-Path $env:APPDATA 'Cursor\auth.json'),
        (Join-Path $env:USERPROFILE '.cursor\auth.json'),
        (Join-Path $env:LOCALAPPDATA 'Cursor\auth.json')
    )) {
        if(!(Test-Path -LiteralPath $path -PathType Leaf)) { continue }
        try {
            $json=Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
            if($json.accessToken) { return [string]$json.accessToken }
            if($json.access_token) { return [string]$json.access_token }
        } catch { }
    }
    return $null
}
function Read-CursorToken($database) {
    if (!(Test-Path -LiteralPath $database -PathType Leaf)) {
        $cliToken=Read-CursorCliToken
        if($cliToken) { return $cliToken }
        throw 'Cursor local login not found'
    }
    if (!('CursorLocalAuth' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class CursorLocalAuth {
    [DllImport("winsqlite3.dll", CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_open_v2(byte[] file,out IntPtr db,int flags,IntPtr vfs);
    [DllImport("winsqlite3.dll", CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_prepare_v2(IntPtr db,byte[] sql,int length,out IntPtr stmt,IntPtr tail);
    [DllImport("winsqlite3.dll", CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_step(IntPtr stmt);
    [DllImport("winsqlite3.dll", CallingConvention=CallingConvention.Cdecl)] static extern IntPtr sqlite3_column_text(IntPtr stmt,int column);
    [DllImport("winsqlite3.dll", CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_column_bytes(IntPtr stmt,int column);
    [DllImport("winsqlite3.dll", CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_finalize(IntPtr stmt);
    [DllImport("winsqlite3.dll", CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_close(IntPtr db);
    [DllImport("winsqlite3.dll", CallingConvention=CallingConvention.Cdecl)] static extern int sqlite3_busy_timeout(IntPtr db,int ms);
    public static string Read(string file) {
        IntPtr db=IntPtr.Zero,stmt=IntPtr.Zero;
        try {
            if(sqlite3_open_v2(Encoding.UTF8.GetBytes(file+"\0"),out db,1,IntPtr.Zero)!=0) throw new Exception("Cannot read Cursor local login database");
            sqlite3_busy_timeout(db,1000);
            var sql=Encoding.UTF8.GetBytes("SELECT value FROM ItemTable WHERE key='cursorAuth/accessToken' LIMIT 1;\0");
            if(sqlite3_prepare_v2(db,sql,-1,out stmt,IntPtr.Zero)!=0) throw new Exception("Cannot query Cursor local login");
            int step=sqlite3_step(stmt);
            if(step==101)return null;
            if(step!=100)throw new Exception("Cursor local login database is busy");
            IntPtr value=sqlite3_column_text(stmt,0);
            if(value==IntPtr.Zero)return null;
            int length=sqlite3_column_bytes(stmt,0);
            byte[] bytes=new byte[length];Marshal.Copy(value,bytes,0,length);
            return Encoding.UTF8.GetString(bytes);
        } finally { if(stmt!=IntPtr.Zero)sqlite3_finalize(stmt); if(db!=IntPtr.Zero)sqlite3_close(db); }
    }
}
'@
    }
    $token=[CursorLocalAuth]::Read($database)
    if (!$token) { $token=Read-CursorCliToken }
    if (!$token) { throw 'Sign in to Cursor first' }
    return $token
}
function Fetch-CursorResponse($settings) {
    $database=$settings.CursorDatabase
    if (!$database) { $database=Join-Path $env:APPDATA 'Cursor/User/globalStorage/state.vscdb' }
    $token=Read-CursorToken $database
    try {
        $parts=$token.Split('.')
        if($parts.Length -lt 2) { throw 'Invalid local Cursor login' }
        $payload=$parts[1].Replace('-','+').Replace('_','/')
        $payload=$payload.PadRight($payload.Length+((4-$payload.Length%4)%4),'=')
        $claims=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($payload)) | ConvertFrom-Json
        $userId=($claims.sub -split '\|')[-1]
        if($userId -notmatch '^[a-zA-Z0-9._-]+$') { throw 'Invalid local Cursor login' }
        if($claims.exp -and [double]$claims.exp -le [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()) { throw 'Cursor login expired: open Cursor and sign in again' }
        [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
        try {
            $headers=@{Authorization="Bearer $token";'Content-Type'='application/json';Accept='application/json'}
            return Invoke-RestMethod -Method Post -Uri 'https://api2.cursor.sh/aiserver.v1.DashboardService/GetCurrentPeriodUsage' -Headers $headers -Body '{}' -TimeoutSec 15 -MaximumRedirection 0
        } catch {
            $session=New-Object Microsoft.PowerShell.Commands.WebRequestSession
            $cookie=New-Object Net.Cookie('WorkosCursorSessionToken',($userId+'%3A%3A'+$token),'/','cursor.com')
            $cookie.Secure=$true
            $session.Cookies.Add($cookie)
            try {
                return Invoke-RestMethod -Uri 'https://cursor.com/api/usage-summary' -WebSession $session -Headers @{Accept='application/json'} -TimeoutSec 15 -MaximumRedirection 0
            } catch { throw 'Cursor usage request failed: open Cursor, check login and network' }
        }
    } finally { $token=$null; $claims=$null; $session=$null; $cookie=$null }
}
