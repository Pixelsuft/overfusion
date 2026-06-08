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

TODO

## Dark mode

TODO

## Extra hooks

TODO

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
