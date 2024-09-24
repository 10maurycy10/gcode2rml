# Userspace driver for Roland CNC mills.

This program converts standard G-code into the RML format used by Roland's CNC mills.
This allows the mills to be used without the aging vendor provided software.

> Note: The MDX-540 mill has 2 modes, "RML" and "NC".
> Both modes accept RML programs, but use different units.
> If the mill is in NC mode, pass the `nc` argment to generate correct toolpaths.

The program only depends on the standard library, so compiling should be fairly straightforward.
This command works for gcc:

```sh
gcc -lm gcode2rml.c -o gcode2rml
```

To use it, just pipe in the G-code and it will output the translated RML:

```sh
./gcode2rml < filetocut.nc > converted.rml
```

Status and error messages will be send to standard error.
Most "unsupported command" warnings can be ignored, but it might be worthwhile to check that omitting those commands doesn't lead to broken tool path.

The RML commands can be sent to the mill by simply printing them:

```
cat converted.rml > /dev/usb/lp0
```

or 

```sh
./gcode2rml < filetocut.nc > /dev/usb/lp0
```

The origin of the toolpath is the starting position of the mill.
Your CAM tool must be set to assume the stock is below the origin.
Alternatively, because the generated RML uses relative coordinates, it's possible to manually move the entire toolpath by inserting or removing a movement command, see the "RML-1 Format" section.

It's also possible to set the mill to an absolute position by pretending a sequence like this:

```
!MC0;
^PA;
!ZEZ0;
!ZEX0Y0;
^PR;
```

This one moves the mill to the origin, starting with the Z axis to hopefully avoid hitting anything.

Passing `flush` on the command line will cause the program to write to the mill after every line, allowing be used for realtime control in place of the mill's planel.

# Supported G-code commands

|Commands|Function|
|-|-|
|G01, G02|Linear movement (Same feed rate used for both)|
|G02, G03|Circular movement (Only with I,J,K)|
|G17, G18, G19|Set circular movement plane|
|G10|Set offset (tool radius offer is not supported)|
|G20, G21|Set units (mm/in)|
|G90, G91|Relative/absolute positioning|
|G49|Clear tool length offset, ignored|
|S|Set spindle speed (approximate, should be within 400 RPM)|
|F|Set feed rate|
|M00, M01, M02, M30|Stop program, ignored|
|M03, M04|Start spindle (Always clockwise)|
|M05|Stop spindle|
|M06|Tool change, ignored|

4-axis machining is only supported for linear movement (G01, G02).

# RML-1 Format

There doesn't seem to be much online about RML, so here are all the commands I tested on my MDX-540.
This list is by no means complete, but includes everything used by the converter.

Each command consists of a command name, followed by a number of comma separated arguments, and ends with a semicolon.
White space between commands is ignored.

Coordinates are in units of 100ths of a millimeter (10 microns), or 100ths of a degree (36 arc seconds). 

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

Same as `V`, but only effects 2d movement, like `PA/D` and `PR/I`.

## PA and D (Plot absolute)

Takes 2 arguments, and moves the mill to those X, Y coordinates in 100ths of a millimeter, leaving Z unchanged.
Running this makes commands that are not explicitly absolute or relative, like `Z` or `!ZE` use absolute coordinates.
It can also be used with no arguments to just switch coordinate modes, without moving.

`^PA`, `D` and `PA` are all valid commands with the same effect.

`^PA1000,2000;`: Move to (10 mm, 20 mm) on X, Y.

`^PA;`: Switch to absolute motion.

## PR and I (Plot relative)

Similar to `PA`, but coordinates are relative to the mills current positions.
Additionally, it will switch to using relative positioning instead of absolute.

`^PR`, `I` and `PR` are all valid commands with the same effect.

`PR1000,0;` Move 10 mm along  X.

`PR;` Switch to relative motion.

## Z (3 axis move)

Takes 3 arguments, X, Y and Z coordinates in 100ths of a millimeter and moves the mill to that position.
Absolute or relative movement modes are set with `PA` and `PR`.
This command uses the feed rate specified by `V`, even if the Z axis movement is zero.

`Z1000,1000,1000`: Move 10 mm along all axes if in relative mode, or to (10, 10, 10) if in absolute mode.

## !MC (Motor control)

Takes 1 argument, starting the spindle if it was a 1, and stopping it if it was a 0.

## !RC (Rate control)

Takes 1 argument, used to set the speed of the spindle. For small values:

```
Speed (RPM) = 400 + 737 * setting
```

For larger ones, the argument is interpreted as speed in RPM.

## !ZE (4 axis move)

This command has a different argument syntax, similar to G-code.
Each argument is prefixed with `X`, `Y`, `Z` or `A`, indicating which axis it corresponds to, and are concatenated without any separators.
This has the same modal coordinate system as the `Z` command.


`!ZEX1`: Move the X axis by/to 10 microns.

`!ZEX1.0`: Move the 4th axis by/to 1 degree

`!ZEX1`: Move the 4th axis by/to 0.01 degree

`!ZEX100Y100Z100A100`: Move X,Y,Z by/to 1 mm, and the 4th axis by/to 1 degree.

## Undocumented commands

These are generated by the vendor provided VPanel software, but I do not know what they do yet.

`!CO[int];`, `!SL[int];`, `!SP[int];`, `!RD[int];`
