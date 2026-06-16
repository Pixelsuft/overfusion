## Requirements

- Git
- Microsoft Visual Studio (with at least C++14 support)
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

## Building the OverFusion injector

1. Open `x86 Native Tools Command Prompt for VS` inside `Visual Studio Tools`
2. Navigate to the `scripts` folder
3. Run `build_injector.bat` script inside the opened terminal
4. Now you have `ofinjector.exe` binary
