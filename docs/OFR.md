## OverFusion Replay format

It's a CSV compatible format.

1st line starts with: `negative number (-4)`,pixelsuft_overfusion,`replay version` <br />
2nd line starts with: `negative number (-3)`,total,`total frames` <br />
3rd line starts with: `negative number (-2)`,rerecords,`rerecord count` <br />
4rd line starts with: `negative number (-1)`,events_begin <br />

Then event lines (sorted by `frame number`): <br />
`frame number`,`event id`,`event data seperated with comma` <br />

`event data` for each `frame number`: <br />
1 (key down/up) (the same VK once per frame at maximum) (no double press/release allowed) - `virtual keycode`,`is down? (1/0)` <br />
2 (mouse down/up) (no double press/release allowed) - `virtual keycode`,`is down? (1/0)` <br />
3 (mouse move) - `normalized x (float) in [0; 1000]`,`normalized y (float) in [0; 1000]` <br />
20 (message box choice) - `message box choice (int)`
