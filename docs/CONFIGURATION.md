## Config file

Create config file `overfusion.json` inside the `path-to-the-game\project-name` folder. <br />
You can use [default config](../overfusion.json) as well.

## Settings

`fps` - game FPS <br />
`force_resolution` - can be used to force game thinking that window resolution is this one (useful for direct video capturing) <br />
`force_custom_window` - force custom window for UI (for Direct3D 9 games) <br />
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
`no_mouse_manipulation` - disallow the game to manipulate the mouse <br />
`draw_cursor` - draw dot in the virtual mouse position (currently works only without the custom window) <br />
`pixel_filter` - make the game look pixelated instead of blurry (only for Direct3D 9 game) <br />
`ui_pixel_filter` - do the same but for OF UI <br />
`save_vfs` - save/load VFS state into state file <br />
`cmdline_append` - extra command line arguments passed to the game <br />
`ffmpeg_cmdline` - FFmpeg command line for video capturing <br />
`binds` - keyboard bindings <br />
