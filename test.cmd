@echo off
setlocal
cd /d "%~dp0"
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "TM_TEST_VS=%%i"
if not defined TM_TEST_VS exit /b 1
call "%TM_TEST_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist artifacts\tests mkdir artifacts\tests
cl /nologo /std:c++20 /utf-8 /EHsc /MT tests\if_table_snapshot_test.cpp /Fo:artifacts\tests\ /Fe:artifacts\tests\if_table_snapshot_test.exe
if errorlevel 1 exit /b 1
artifacts\tests\if_table_snapshot_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /MT /Iinclude tests\builtin_features_test.cpp TrafficMonitor\Builtin\BuiltinFeatures.cpp TrafficMonitor\Builtin\AIUsage.cpp TrafficMonitor\Builtin\DualCountdown.cpp TrafficMonitor\Builtin\Power.cpp /Fo:artifacts\tests\ /Fe:artifacts\tests\builtin_features_test.exe /link shell32.lib user32.lib
if errorlevel 1 exit /b 1
artifacts\tests\builtin_features_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++20 /utf-8 /EHsc /MT tests\stable_item_colors_test.cpp /Fo:artifacts\tests\ /Fe:artifacts\tests\stable_item_colors_test.exe
if errorlevel 1 exit /b 1
artifacts\tests\stable_item_colors_test.exe
if errorlevel 1 exit /b 1
python tests\config_migration_test.py
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File tests\ai_usage_test.ps1
if errorlevel 1 exit /b 1
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1 -Configuration "Debug (lite)"
if errorlevel 1 exit /b 1
python tests\taskbar_smoke.py "artifacts\build\x64\Debug (lite)\TrafficMonitor.exe"
if errorlevel 1 exit /b 1
python tests\compact_ai_smoke.py "artifacts\build\x64\Debug (lite)\TrafficMonitor.exe"
if errorlevel 1 exit /b 1
python tests\reset_alert_smoke.py "artifacts\build\x64\Debug (lite)\TrafficMonitor.exe"
exit /b %errorlevel%
