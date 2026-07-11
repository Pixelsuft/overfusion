## OverFusion Replay format

It's a CSV compatible format. You can even use Excel to modify replays! <br />

First lines (order may (unlikely) be different except for `events_begin`): <br />
1st line starts with: `negative number (-9)`,pixelsuft_overfusion,`replay version` <br />
2nd line starts with: `negative number (-8)`,total,`total frames` <br />
3rd line starts with: `negative number (-7)`,rerecords,`rerecord count` <br />
4th line starts with: `negative number (-6)`,fps,`FPS (info)` <br />
4th line starts with: `negative number (-5)`,delta_multiplier,`Delta multiplier (info)` <br />
5th line starts with: `negative number (-4)`,system_offset,`system time offset in ms (info)` <br />
6th line starts with: `negative number (-3)`,local_offset,`local time offset in ms (info)` <br />
7th line starts with: `negative number (-2)`,startup_offset,`startup time offset in ms (info)` <br />
8th line starts with: `negative number (-1)`,events_begin,`0` <br />

Then event lines (sorted by `frame number`): <br />
`frame number`,`event id`,`event data seperated with comma` <br />

`frame number` with the corresponding `event data`: <br />
`1` (key down/up) (the same VK once per frame at maximum) (no double press/release allowed) - `virtual keycode`,`is down? (1/0)` <br />
`2` (mouse down/up) (no double press/release allowed) - `virtual keycode`,`is down? (1/0)` <br />
`3` (mouse move) - `normalized x (float) in [0; 1000]`,`normalized y (float) in [0; 1000]` <br />
`4` (hash check) - `current internal RNG seed for hopefully catching desyncs (only RNG for now)` <br />
`5` (set RNG seed) - `set internal RNG seed (RNG manipulation)` <br />
`6` (push value to the RNG buffer) - `RNG range`,`RNG value`,`repeat count` <br />
`7` (pop value from RNG buffer with specific range) - `RNG range or 0 to fully clear RNG buffer` <br />
`20` (message box choice) - `message box choice (int)`

Example replay:

```csv
-8,pixelsuft_overfusion,1
-7,total,1145
-6,rerecords,2
-5,fps,60
-4,system_offset,0
-3,local_offset,0
-2,startup_offset,0
-1,events_begin,0
757,1,40,1
763,1,40,0
769,1,40,1
774,1,40,0
813,1,40,1
826,1,40,0
959,3,197.265625,855.468750
992,2,1,1
1004,2,1,0
```
