This program converts standard G-code into RML the format used by Roland's CNC mills.
The output can then be directly sent to the mill, simply by printing it. (The mill emulates a line printer)

The program only depends on the standard library, so compiling should be fairly straightforward.
This command should work if you have gcc installed:

```sh
gcc -lm gcode2rml.c -o gcode2rml
```

To use it, just pipe in the G-code and it will output the translated RML:

```sh
./gcode2rml < filetocut.nc > converted.rml
```

On linux (and other unix), it's possible to directly send the output to the mill:

```sh
./gcode2rml < filetocut.nc > /dev/usb/lp0
```

This program always uses relative positioning, the mill has to be manually moved to the desired origin before cutting.

For some reason, my CNC mill does not cut circles properly (very slow and uneven feed) except on the first file sent.
As a workaround, send any G-code, and while it's running, manually hold "Clear" to reset the mill without reinitializing it.

# Supported G-code commands

|Commands|Function|
|-|-|
|G01, G02|Linear movement (Same feed rate used for both)|
|G02, G03|Circular movement (Only when I,J,K are used, explicit radius is not supported)|
|G17, G18, G19|Set circular movement plane|
|G10|Set offset (only for X, Y and Z)|
|G20, G21|Set units (mm/in)|
|G90, G91|Relative/absolute positioning|
|G49|Clear tool length offset, ignored|
|S|Set spindle speed (approximate, should be within 400 RPM)|
|F|Set feed rate|
|M00, M01, M02, M30|Stop program, ignored|
|M03, M04|Start spindle (Always clockwise)|
|M05|Stop spindle|
|M06|Tool change, ignored|

4-axis machining is not supported.
Automatic tool changes are not supported, but many CAM tools will generate M06 and G49 commands even for a single tool job.

# RML-1 Format

There doesn't seem to be much online about RML, so here are all the commands I tested on a MDX-540.
Each command consists of a command name, followed by a number of comma separated arguments, and ends with a semicolon.
White space before and after commands is ignored. 

Example program:

```
V60;
^PR;
!RC15;
!MC1;
Z0,0,-1000;
Z0,0,1000;
!MC0;
```

This will start the spindle at 12000 RPM, cut 10 mm down, move back up and stop the spindle.

## V (set feed rate)

Sets the feed rate used for 3 dimensional movement commands in mm/second.
Does not affect 2d movement like `PR` or `PA`


`V10;`: Set the feed rate to 10 mm/second or 60 mm/minute.

## F (set 2d feed rate)

Same as `V`, but only effects 2d movement.

## PA and D (Plot absolute)

Takes 2 arguments, and moves the mill to those X, Y coordinates in 100ths of a millimeter.
Does not change Z position.
Makes the `Z` command use absolute coordinates, even if called with no arguments.

`PA1000,2000;`: Move to (10 mm, 20 mm) on X, Y.

`PA;`: Switch `Z` to absolute motion.

## PR and I (Plot relative)

Similar to `PA`, but coordinates are relative to the mills current positions.
Running this will make the `Z` command relative.

`PR1000,0;` Move 10 mm along  X.

`PR;` Switch `Z` to relative motion.

## Z (3 axis move)

Takes 3 arguments, X, Y and Z coordinates in 100ths of a millimeter and moves the mill to that position.
Coordinates are relative if `PR` was executed, and absolute if `PA` was executed.
This command uses the feed rate specified by `V`, even if the Z axis movement is zero.

`Z1000,1000,1000`: Move 10 mm along all axes if in `PR` mode, or to (10, 10, 10) if in `PA` mode.

## !MC (Motor control)

Takes 1 arguments, starting the spindle if it was a 1, and stopping it on a zero.

## !RC (Rate control)

Takes one argument, used to set the speed of the spindle. For small values:

```
Speed (RPM) = 400 + 737 * setting
```

For larger ones, the argument is interpreted as RPM.
