param([string]$Destination='C:\_FLEX\TrafficMonitor',[switch]$SkipBuild,[switch]$NoStart)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if (!$SkipBuild) { & (Join-Path $root 'build.ps1') }
$built=Join-Path $root 'artifacts\build\x64\Release (lite)'
if (!(Test-Path -LiteralPath (Join-Path $built 'TrafficMonitor.exe'))) { throw 'Build the Lite Release first.' }
$destinationPath=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
if (!(Test-Path -LiteralPath $destinationPath)) { throw 'Destination must already exist.' }
$commit=(& git -C $root rev-parse --short HEAD).Trim()
$backupRoot=$destinationPath+'-archive'
$backup=Join-Path $backupRoot ('source-deploy-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
if (!$backup.StartsWith($backupRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid backup location.' }
New-Item -ItemType Directory -Path $backup -Force | Out-Null
$programConfig=Join-Path $destinationPath 'config.ini'
$globalConfig=Join-Path $destinationPath 'global_cfg.ini'
$selection=$null
# One-time, read-only import from the previous installation; runtime never reads AppData.
if ((Test-Path -LiteralPath $globalConfig) -and ((Get-Content -LiteralPath $globalConfig -Raw) -match 'portable_mode\s*=\s*false')) {
    $oldSelection=Join-Path $env:APPDATA 'TrafficMonitor\config.ini'
    if(Test-Path -LiteralPath $oldSelection) { $selection=$oldSelection }
}
if (!('TaskbarDeploymentWindow' -as [type])) {
Add-Type -TypeDefinition 'using System; using System.Text; using System.Runtime.InteropServices;
public static class TaskbarDeploymentWindow {
 delegate bool Callback(IntPtr h,IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumWindows(Callback cb,IntPtr p);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder name,int max);
 [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h,uint msg,IntPtr w,IntPtr l);
 public static void CloseCore(int target) {
  EnumWindows((h,p)=> {uint pid;GetWindowThreadProcessId(h,out pid);var name=new StringBuilder(128);GetClassName(h,name,128);
   if(pid==target && name.ToString()=="TrafficMonitor_r7XZaS4p") PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);return true;},IntPtr.Zero);
 }
}'
}
$binary=Join-Path $destinationPath 'TrafficMonitor.exe'
$running=@(Get-Process TrafficMonitor -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $binary})
foreach($instance in $running) {
    [TaskbarDeploymentWindow]::CloseCore($instance.Id)
    if(!$instance.WaitForExit(12000)) {throw 'Installed instance did not close; deployment stopped before replacement.'}
}
foreach($name in @('TrafficMonitor.exe','config.ini','global_cfg.ini','build.ps1','README.md','.deployment.json')) {
    $file=Join-Path $destinationPath $name
    if(Test-Path -LiteralPath $file) {Copy-Item -LiteralPath $file -Destination $backup}
}
if($selection) {Copy-Item -LiteralPath $selection -Destination (Join-Path $backup 'previous-active-selection.ini')}
if(Test-Path -LiteralPath $programConfig) {
    $migration=@((Join-Path $root 'tools\migrate_config.py'),$programConfig)
    if($selection) {$migration+=@('--selection-profile',$selection)}
    & python @migration
    if($LASTEXITCODE -ne 0) {throw 'Configuration migration failed; original files remain in the backup.'}
}
Copy-Item -LiteralPath (Join-Path $built 'TrafficMonitor.exe') -Destination $binary -Force
$sensorTarget=Join-Path $destinationPath 'sensors'
New-Item -ItemType Directory -Path $sensorTarget -Force | Out-Null
Copy-Item -Path (Join-Path $built 'sensors\*') -Destination $sensorTarget -Force
$helperTarget=Join-Path $destinationPath 'plugins'
New-Item -ItemType Directory -Path $helperTarget -Force | Out-Null
# Only source-controlled helpers; never copy credentials, INI settings or caches.
foreach($helper in Get-ChildItem -LiteralPath (Join-Path $root 'helpers\AIUsage') -File) {
    $old=Join-Path $helperTarget $helper.Name
    if(Test-Path -LiteralPath $old) {Copy-Item -LiteralPath $old -Destination $backup -Force}
    Copy-Item -LiteralPath $helper.FullName -Destination $old -Force
}
@{source=$root;commit=$commit;configuration='Release (lite)';configurationDirectory=$destinationPath;deployedAt=(Get-Date).ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $destinationPath '.deployment.json') -Encoding UTF8
$readme=(Get-Content -LiteralPath (Join-Path $root 'install\README.md') -Raw -Encoding UTF8).Replace('{{SOURCE_DIR}}',$root)
Set-Content -LiteralPath (Join-Path $destinationPath 'README.md') -Value $readme -Encoding UTF8
if(!$NoStart) {
    . (Join-Path $root 'tools\start_installation.ps1')
    Start-TrafficMonitorInstallation -Executable $binary
}
Write-Host "Deployed Lite source commit $commit to $destinationPath"
Write-Host "Program-directory configuration; backup: $backup"
