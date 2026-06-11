# Extending game support for the OverFusion

If you port game for the first time, I strongly recommend you to try porting already ported game first.

## Requirements

- Ghidra
- Cheat Engine (or any other dynamic analysis tool)

## FNAF3 porting example

In the plugins folder, copy `fnaf.cpp` into `fnaf3.cpp` (since there is no template yet, we will just adopt another plugin). Add it to the Visual Studio project inside the `Plugins` section and put it into the jumbo build file `jumbo/main.cpp`. Edit `fnaf3.cpp`: rename `PlugFnaf` class into `PlugFnaf3` (and change `name` variable for the class), `on_plugin_check_fnaf` into `on_plugin_check_fnaf3`, change exe name to `FiveNightsatFreddys3.exe` in it. Comment all memory writes, change all offsets to 0 to not forget to implement something. Change default FPS to your game FPS. <br />
Obtain the game. Create `FNAF3` folder on my drive and put `FiveNightsatFreddys3.exe` into it. Now we need to dump all the plugins. Open `%temp%` folder (cleanup it to make finding easier), run the game, see newly created folder with name like `mrt460C.tmp` or something (inside the temp folder), copy it to our `FNAF3` folder and rename to `dump`, close the game <br />
Now let's create a Ghidra project. Set project dir to our `FNAF3` folder and let's call a project `defnaf3`. Add game exe to it (drag). Then double click on it, start analyzing it, wait. Save the project. <br />
Go to `Symbol Tree`->`Imports`->`COMDLG32.dll`->`GetSaveFileNameA`/`GetSaveFileNameW`->Right click->`Show References to`. Find a function which uses it, rename it into `ShowStateSaveDialog`. Now find references to `ShowStateSaveDialog` by right clicking on it. You can find a big function which uses it. Rename it into `SaveGameState`. Remember it's offset `48080` and calling conversion `__fastcall`. <br />
![porting1](../screenshots/porting1.png) <br />
Let's modify our plugin:

```cpp
void(__fastcall* SaveGameState)(void* hfile);
```

```cpp
SaveGameState = reinterpret_cast<decltype(SaveGameState)>(mem::get_base() + 0x48080);
```

Now do the same for loading a state. Find and rename `ShowStateLoadDialog` function by searching `GetOpenFileName` refs, then find a big (!!!) function which uses `ShowStateLoadDialog` and rename it into `LoadGameState`. Let's modify our plugin again (please keep remembering that I'm porting FNAF3 as an example and every single game (and different game versions) runtime likely has unique offsets):

```cpp
void(__fastcall* LoadGameState)(void* hfile);
```

```cpp
LoadGameState = reinterpret_cast<decltype(LoadGameState)>(mem::get_base() + 0x49c70);
```

Find a function which refs both `SaveGameState` and `LoadGameState` (using the same `Find References to` tool) and looks like this: <br />
![porting2](../screenshots/porting2.png) <br />
Rename it into `UpdateGameFrame` and remember offset:

```cpp
cfg.pUpdateGameFrame = reinterpret_cast<void*>(mem::get_base() + 0x46010);
```

Rename red function into `ExecuteEvents` and green into `ExecuteTriggeredEvent`: <br />
![porting3](../screenshots/porting3.png)

## TODO

finish this doc
