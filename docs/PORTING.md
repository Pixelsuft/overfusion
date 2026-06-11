# Extending game support for the OverFusion

If you port game for the first time, I strongly recommend you to try porting already ported game first.

## Requirements

- Ghidra
- Cheat Engine (or any other dynamic analysis tool)

## FNAF3 porting example

In the plugins folder, copy `fnaf.cpp` into `fnaf3.cpp` (since there is no template yet, we will just adopt another plugin). Add it to the Visual Studio project inside the `Plugins` section and put it into the jumbo build file `jumbo/main.cpp`. Edit `fnaf3.cpp`: rename `PlugFnaf` class into `PlugFnaf3` (and change `name` variable for the class), `on_plugin_check_fnaf` into `on_plugin_check_fnaf3`, change exe name to `FiveNightsatFreddys3.exe` in it. Comment all memory writes, change all offsets to 0 to not forget to implement something. <br />
Obtain the game. Create `FNAF3` folder on my drive and put `FiveNightsatFreddys3.exe` into it. Now we need to dump all the plugins. Open `%temp%` folder (cleanup it to make finding easier), run the game, see newly created folder with name like `mrt460C.tmp` or something (inside the temp folder), copy it to our `FNAF3` folder and rename to `dump`, close the game <br />
Now let's create a ghidra project. Set project dir to our `FNAF3` folder and let's call a project `defnaf3`. Add game exe to it (drag). Then double click on it, start analyzing it, wait. Save the project.

## TODO

finish this doc
