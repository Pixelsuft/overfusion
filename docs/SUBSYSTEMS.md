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
3. Allow title bar context menu and window submenus to be dark
4. Draw dark window menu manually (if needed)
5. Draw dark window background manually (if needed)
6. Manually fix message boxes via CBT hook and window subclass (if needed)
7. Try to allow dark mode for Win32 UI elements
8. Detect theme preference change

## Extra hooks

1. Disable dragging from/into window
2. Disable opening URLs, explorer, etc
3. Emulate US locale
4. Emulate fixed Windows version
5. Emulate lack of any joystick
6. Emulate disabled networking
7. Emulate fixed username
8. Disable registry access
9. Emulate custom command line arguments

## Virtual filesystem

Optional in-memory FS emulation (set `virtual_fs` to `true` to enable)

1. Hooks for `CreateFile`, `OpenFile`, `_lopen`, `_open`, `_fopen` and other related functions
2. Legacy INI API emulation (very slow via [SimpleIni](https://github.com/brofield/simpleini))
3. Saving/loading support for states

## Game hooks

1. Game cycle update hooking/hijacking
2. Transition processing hooking/hijacking
3. Late initialization during first frame update
4. Forcing game to pause/advance
5. Communication with video recording subsystem

## GDI hooks

Blitting operating hooks to support direct video recording when the game uses GDI backend

## Input

1. Emulation of the keyboard and mouse state
2. Mapping key codes from and to string
3. Bindings logic
4. Helper functions

## Load hooks

`LoadLibrary`, `GetProcAddress` hooks for observating (and faking) some imports and IAT patching

## Plugins

Some sort of the game backends

1. Should provide functions to detect whether their game is running
2. Should implement init functions
3. Should implement getting pointers to the game data
4. Should implement in-game state saving/loading
5. Should implement mouse coordinates convertion
6. Can do some extra operations/hooks/patches

## State

1. Replay recording/playback logic
2. Save data saving/loading logic
3. Replay exporing/importing
4. TAS event logic
5. UI info displaying logic
6. Interaction with other subsystems

## Thread hooks

Disable subprocess creation and extra thread for determinism if `disable_threads` is `true`

## Time hooks

Time hooks for deterministic system time emulation and optionally timers

## UI

Main OF user interface code utilizing the [ImGui](https://github.com/ocornut/imgui) library

## Video

Video capturing logic. See more [here](CAPTURING.md)

## Window hooks

1. Window creation hooks
2. Event processing hooks (for UI, dark mode, input manipulation, determinism)
3. Message boxes hooks for manipulation
4. Some extra hooks for determinism

## Jumbo (unity) build

Is used to build all the source files as a single one for faster compilation from strach and runtime performance
