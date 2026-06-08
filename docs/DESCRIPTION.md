## Injector

How does injector work? <br />

1. It spawns a game child process in a suspended state
2. Injects OF DLL in the spawned process using the LoadLibrary method
3. Waits till the DLL sets up
4. Resumes the game process

## Audio

DirectSound proxy used for capturing audio. See more [here](CAPTURING.md)

## Config

Config parsing, binds parsing, filling default values. See more [here](CONFIGURATION.md)

## Custom window

Creates custom window for OF ImGui UI in case the game doesn't use Direct3D9 render or `force_custom_window` is set to `true`

## Direct3D hooks

Direct3D9 hooks for drawing OF ImGui UI inside the game window if possible

## Dark mode

Ugly piece of code for supporting dark theme (can be disabled with `disable_dark_mode_support`)

1. Detect system theme
2. Apply dark theme on titlebar (if needed)
3. Allow title bar context menu and window sub-menus to be dark
4. Draw dark window menu manually (if needed)
5. Draw dark window background manually (if needed)
6. Manually fix message boxes via CBT hook and window subclass (if needed)
7. Try to allow dark mode for Win32 UI elements
8. Detect theme preference change

## Extra hooks

1. Disable dragging from/into window
2. Disable opening URLs, explorer, etc.
3. Emulate US locale
4. Emulate fixed Windows version
5. Emulate lack of any joystick
6. Emulate disabled networking
7. Emulate fixed username
8. Disable registry access
9. Emulate custom command line arguments

## Virtual FS

TODO

## Game hooks

TODO

## GDI hooks

TODO

## Input

TODO

## Load hooks

TODO

## Plugins

TODO

## State

TODO

## Thread hooks

TODO

## Time hooks

TODO

## UI

TODO

## Window hooks

TODO

## Jumbo (unity) build
