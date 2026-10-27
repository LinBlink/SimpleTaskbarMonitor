# Taskbar edition FAQ

## How do I enable the display?

It opens automatically when the application starts. The old `show_task_bar_wnd=false` setting is ignored.

## How do I configure it?

Right-click the taskbar display or tray icon. **Display Settings** selects and orders items; **Options** configures appearance, units and layout. Options contains taskbar and general settings.

## Why is the display missing or overlapping other items?

Open Options from the tray menu and check the enabled items, taskbar position and secondary display settings. Windows 11 settings include margins, offsets and widget avoidance. The application recreates its display when Explorer restarts.

Failed embedding is retried and reported. You can exit and restart through the tray menu.

## Why is network speed always zero?

Refresh the adapter list under **Select Network Connection**, select the active adapter, or use automatic selection. General settings controls hidden connections and whether all interfaces are shown.

## Where are skins, the floating window and historical statistics?

They have been removed in this taskbar edition, along with network details and update checks. Today's traffic is kept in memory and resets when the program restarts.

## How do I use plugins or temperature items?

Place compatible plugin DLLs in the `plugins` directory beside the executable, restart, then configure Plugin Manager and Display Settings. Plugin architecture must match the executable. Lite includes CPU/GPU temperature collection through the bundled isolated worker. Configuration is always saved in the program directory.

## How do I quit?

Choose **Exit** from the context menu. Closing the taskbar display also exits, saves settings, removes the tray icon and restores taskbar space.

See the [README](./README_en-us.md) for the feature scope and build instructions.
