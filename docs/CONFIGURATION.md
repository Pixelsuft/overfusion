## Config file

Create config file `overfusion.json` inside the game folder and `ofproject.json` project config inside the project folder (located in the game folder). <br />
You can use [default config](../overfusion.json) and [default project config](../ofproject.json) as well.

## Settings (overfusion.json)

`force_resolution` - can be used to force game thinking that window resolution is this one (useful for direct video capturing) <br />
`speed` - game speed in range [0.05, 2.0] <br />
`font_scale` - UI scale in range [0.05, 3.0] <br />
`force_custom_window` - force custom window for UI (for Direct3D 9 games) <br />
`disable_fullscreen` - prevent game from entering into fullscreen mode <br />
`wait_for_debugger` - wait till the debugger attached before running a game <br />
`show_menu` - show OF menu by fault <br />
`show_info` - show OF info window by default <br />
`emulate_user_timers` - manually emulate `SetTimer` timers <br />
`emulate_mm_timers` - manually emulate `timeSetEvent` timers <br />
`virtual_fs` - emulate FS in memory <br />
`disable_threads` - disable thread creation <br />
`delay_thread_hook` - disable thread creation only after the game loaded <br />
`no_ini_hooks` - disable slow legacy ini function emulations for VFS <br />
`boxed_mode` - non-stop running without emulating timer functions <br />
`disable_dark_mode_support` - disable dark mode support <br />
`disable_app_menu` - disable game window menu <br />
`allow_timers_fix` - allow in-game timers to be fixed during state loading <br />
`allow_direct_capture` - allow capturing video from the game directly (if the game uses Direct3D 9 or GDI)<br />
`allow_audio_hook` - allow hooking the audio subsystem <br />
`disable_audio` - disable audio playback completely <br />
`record_audio` - record in-game audio <br />
`support_audio_panning` - support panning for recording audio <br />
`draw_cursor` - draw dot in the virtual mouse position (currently works only without the custom window) <br />
`pixel_filter` - make the game look pixelated instead of blurry (only for Direct3D 9 games) <br />
`ui_pixel_filter` - do the same but for OF UI <br />
`save_vfs` - save/load VFS state into state file <br />
`pause_on_scene_switch` - pause the game when scene switches (or gets reset) <br />
`disable_viewport` - disable `Viewport.mfx` Fusion plugin display <br />
`disable_perspective` - disable `Perspective.mfx` Fusion plugin display <br />
`cmdline_append` - extra command line arguments passed to the game <br />
`ffmpeg_cmdline` - FFmpeg command line for video capturing <br />
`binds` - keyboard bindings <br />

## Settings (ofproject.json)

`fps` - game FPS <br />
`delta_multiplier` - delta time multiplier (reverse of FPS) <br />
`system_time_offset`, `local_time_offset`, `startup_time_offset` - emulated virtual time (in ms, relative to when starting a game) <br />
`no_mouse_manipulation` - disallow the game to manipulate the mouse

## Bindings

`task` triggers by pressing `key` bind: <br /> <br />
`map` - emulate (map) keyboard key to the in-game keyboard press `target` <br />
`mouse_down` - toggle virtual mouse button `target` down/up <br />
`mouse_move` - virtual cursor mouse to the real mouse position <br />
`fast` - fast forward (use `toggle` to toggle this mode instead of holding the `key`) <br />
`play` - play (unpause) the game (use `toggle` to toggle this mode instead of holding the `key`) <br />
`replay_mode` - switch to/from replay mode (`value`: 0 to toggle, 1 to replay, -1 to record) <br />
`reset_game` - restart the game and reset state <br />
`advance` - frame advance <br />
`menu` - toggle OF menu <br />
`save` - save game state to slot `slot` <br />
`load` - load game state from slot `slot`

## Default bindings

In-game key maps: `Left`, `Right`, `Up`, `Down`, `Enter`, `R`, `S`, `Z`, `X`, `Del` <br />
`I` - maps to `F2` <br />
`O` - maps to `F3` <br />
`P` - maps to `Esc` <br />
`M` - mouse down/up toggle <br />
`N` - move in-game cursor to the position of the real cursor <br />
`Insert` - toggle menu <br />
`Tab` - fast forward <br />
`Space` - playback <br />
`Pause` - toggle playback <br />
`V` - frame advance <br />
`Q` - switch to/from replay mode <br />
`F12` - reset game <br />
`F1` ... `F9` - save state <br />
`1` ... `9` - load state
