## Requirements

- Git
- Microsoft Visual Studio (at least 2019, but can be 2015 for Windows XP jumbo build)
- Python

## Clonning the repository

```sh
git clone --recursive https://github.com/Pixelsuft/overfusion
```

Now you have `overfusion` folder

## Building the OverFusion DLL (Debug)

1. Open `overfusion.sln` with Visual Studio
2. Choose `Debug` `x86` mode
3. Build the solution: `Build`->`Build Solution` (`Ctrl`+`Shift`+`B`)
4. Now you have `overfusion.dll` in the `Debug` folder

## Building the OverFusion DLL (Release)

1. Run `gen_jumbo.py` script (located in the `scripts` folder)
2. Navigate to the `jumbo` folder
3. Open `overfusion.sln` with Visual Studio
4. Choose `ReleaseFast` `x86` mode
5. Build the solution: `Build`->`Build Solution` (`Ctrl`+`Shift`+`B`)
6. Now you have `overfusion.dll` in the `ReleaseFast` folder
7. Optionally run `patch_win7.py` script and pass path to the `overfusion.dll` as the first argument to it to fix Windows 7 compatibility

## Building the OverFusion DLL (Release) for Windows XP

Just do the same, but use Visual Studio 2015 and open `overfusion.sln` project from the `jumbo\xp` folder. Make sure you have `v140_xp` toolset installed.

## Building the OverFusion injector

1. Open `x86 Native Tools Command Prompt for VS` inside `Visual Studio Tools`
2. Navigate to the `utils` folder
3. Run `build_injector.bat` script inside the opened terminal
4. Now you have `ofinjector.exe` binary
