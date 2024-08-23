This program converts G-code, the standard CNC control language into RML, used by Roland's CNC mills.
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

On linux, it's possible to directly send the output to the mill:

```sh
./gcode2rml < filetocut.nc > /dev/usb/lp0
```

The program always uses relative positioning, the mill has to be manually moved to the desired origin before cutting.

For some reason, my CNC mill does not cut circles properly except on the first file sent.
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
|S|Set spindle speed (approximate, should be within 400 RPM)|
|F|Set feed rate|
|M00, M01, M02, M30|Stop program, Ignored|
|M03, M04|Start spindle (Always clockwise)|
|M05|Stop spindle|

Circular movement is mostly untested, may not work as expected.
