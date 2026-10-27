# TrafficMonitor Lite taskbar edition

This is the authoritative source repository. Build, test and commit here before deploying to `C:\_FLEX\TrafficMonitor`. Only Lite configurations are supported by the build/deploy scripts.

All configurations are stored beside the executable. Neither the working directory nor AppData is used, even when the old `portable_mode` flag is false. Text colors and presets include stable item IDs so adding or reordering AI/temperature items does not shift their colors.

AI usage, dual countdown, battery and power are compiled into the host. CPU/GPU temperatures are new Lite functionality, collected by an isolated worker built from `helpers/TemperatureWorker.cs` and the bundled LibreHardwareMonitor library. The host retains `WITHOUT_TEMPERATURE`, does not load the old hardware API DLL and does not run an older application.

Use `build.ps1`, `test.cmd`, then `deploy.ps1`. Output goes to `artifacts/build/x64/`. Deployment backs up existing configuration and records the source commit. Account credentials and caches are never copied from the source tree. See the [Chinese README](./README.md) for detailed paths, tests and migration behavior.
