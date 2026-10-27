param(
    [ValidateSet('Debug (lite)','Release (lite)')][string]$Configuration='Release (lite)'
)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Install Visual Studio C++ tools, MFC and Windows SDK first.' }
$version=& $vswhere -latest -products '*' -property installationVersion
$toolset=if ([version]$version -ge [version]'18.0') {'v145'} else {'v143'}
$msbuild=Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
$output=Join-Path $root "artifacts\build\x64\$Configuration\"
& $msbuild (Join-Path $root 'TrafficMonitor\TrafficMonitor.vcxproj') /m /nologo /v:minimal "/p:Configuration=$Configuration" /p:Platform=x64 "/p:PlatformToolset=$toolset" "/p:OutDir=$output"
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
$sensors=Join-Path $output 'sensors'
New-Item -ItemType Directory -Path $sensors -Force | Out-Null
$library=Join-Path $root 'OpenHardwareMonitorApi\LibreHardwareMonitorLib.dll'
$csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
& $csc /nologo /target:exe /platform:x64 /optimize+ "/reference:$library" "/out:$sensors\TemperatureWorker.exe" (Join-Path $root 'helpers\TemperatureWorker.cs')
if ($LASTEXITCODE -ne 0) { throw "Sensor worker build failed: $LASTEXITCODE" }
Copy-Item -LiteralPath $library -Destination $sensors -Force
$pluginData=Join-Path $output 'plugins'
New-Item -ItemType Directory -Path $pluginData -Force | Out-Null
Copy-Item -Path (Join-Path $root 'helpers\AIUsage\*') -Destination $pluginData -Force
Write-Host "Built Lite executable and isolated sensor worker: $output"
