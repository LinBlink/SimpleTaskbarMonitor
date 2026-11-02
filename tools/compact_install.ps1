param([string]$Destination='C:\_FLEX\TrafficMonitor',[string]$ArchiveRoot)
$ErrorActionPreference='Stop'
$target=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
if (!(Test-Path -LiteralPath (Join-Path $target 'SimpleTaskbarMonitor.exe')) -or !(Test-Path -LiteralPath (Join-Path $target '.deployment.json'))) {throw 'Expected a source-deployed installation.'}
if (!$ArchiveRoot) {$ArchiveRoot=$target+'-archive'}
$archiveBase=[IO.Path]::GetFullPath($ArchiveRoot).TrimEnd('\')
if ($archiveBase -eq $target -or $archiveBase.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Archive must be outside the installation.'}
$archive=Join-Path $archiveBase ('cleanup-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
if(Test-Path -LiteralPath $archive) {throw 'Archive already exists.'}
New-Item -ItemType Directory -Path $archive -Force | Out-Null
$names=@('artifacts','build','src','CountdownPlugin','UsagePlugin','skins',
    'TrafficMonitorSkinEditor.exe','OpenHardwareMonitorApi.dll','LibreHardwareMonitorLib.dll',
    'test.cmd','build.ps1','FAQ.url','Home.url','tm.dat','error.log','history_traffic.dat.bak')
# Exact allowlist only. Keep all feature settings, credentials, helpers, languages and sensors.
$before=@(Get-ChildItem -LiteralPath $target -Recurse -File -Force | Measure-Object Length -Sum)[0].Sum
Add-Type -TypeDefinition 'using System; using System.Text; using System.Runtime.InteropServices;
public static class CompactInstallationWindow {
 delegate bool Callback(IntPtr h,IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumWindows(Callback cb,IntPtr p);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder name,int max);
 [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h,uint msg,IntPtr w,IntPtr l);
 public static void Close(int target) {EnumWindows((h,p)=> {uint pid;GetWindowThreadProcessId(h,out pid);var name=new StringBuilder(128);GetClassName(h,name,128);
 var cls=name.ToString();if(pid==target && (cls=="SimpleTaskbarMonitor_r7XZaS4p" || cls=="TrafficMonitor_r7XZaS4p")) PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);return true;},IntPtr.Zero);}
}'
$binary=Join-Path $target 'SimpleTaskbarMonitor.exe'
$legacyBinary=Join-Path $target 'TrafficMonitor.exe'
$running=@(Get-Process -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $binary -or $_.Path -eq $legacyBinary})
foreach($instance in $running) {
    [CompactInstallationWindow]::Close($instance.Id)
    if(!$instance.WaitForExit(12000)) {throw 'Application did not close; no runtime files moved.'}
}
try {
    $preserved=@{}
    $keep=@('SimpleTaskbarMonitor.exe','config.ini','global_cfg.ini','history_traffic.dat')
    $keep+=@(Get-ChildItem -LiteralPath $target -Directory | Where-Object {$_.Name -in @('plugins','sensors','language')} | ForEach-Object {Get-ChildItem -LiteralPath $_.FullName -Recurse -File} | ForEach-Object {$_.FullName.Substring($target.Length+1)})
    foreach($name in $keep) {
        $file=Join-Path $target $name
        if(Test-Path -LiteralPath $file -PathType Leaf) {$preserved[$name]=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}
    }
    $manifest=@()
    foreach($name in $names) {
        $path=[IO.Path]::GetFullPath((Join-Path $target $name))
        $out=[IO.Path]::GetFullPath((Join-Path $archive $name))
        if(!$path.StartsWith($target+'\',[StringComparison]::OrdinalIgnoreCase) -or !$out.StartsWith($archive+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Unsafe move target.'}
        if(Test-Path -LiteralPath $path) {
            Move-Item -LiteralPath $path -Destination $out
            $manifest+=$name
        }
    }
    $git=Join-Path $target '.git'
    if(Test-Path -LiteralPath $git -PathType Container) {
        $externalGit=Join-Path $archive 'repository.git'
        # Git handles relocating its metadata and leaves a small .git pointer file.
        & git -C $target init --quiet --separate-git-dir $externalGit
        if($LASTEXITCODE -ne 0) {throw 'Could not relocate Git metadata.'}
        $manifest+='.git -> repository.git'
    }
    foreach($name in $preserved.Keys) {
        if((Get-FileHash -LiteralPath (Join-Path $target $name) -Algorithm SHA256).Hash -ne $preserved[$name]) {throw "Preserved runtime file changed: $name"}
    }
    Write-Host 'Verified: executable, configurations, feature data and runtime dependencies unchanged.'
    @{installation=$target;archivedAt=(Get-Date).ToString('o');moved=$manifest;bytesBefore=$before} |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $archive 'manifest.json') -Encoding UTF8
    $after=@(Get-ChildItem -LiteralPath $target -Recurse -File -Force | Measure-Object Length -Sum)[0].Sum
    Write-Host ('Compacted: {0:N2} MB -> {1:N2} MB' -f ($before/1MB),($after/1MB))
    Write-Host "Archive: $archive"
} finally {
    if($running.Count) {
        . (Join-Path $PSScriptRoot 'start_installation.ps1')
        Start-SimpleTaskbarInstallation -Executable $binary
    }
}
