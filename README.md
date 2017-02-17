# Leibniz
Work in progress emulator for RUNT based Newtons (OMP, Expert Pad, etc)

## Status

### NewtonOS 

All ROM images are able to boot into NewtonOS.

All ROM images except for the ARMistice and Notepad will put the device to sleep.  When this happens the screen will dim.  There's a power switch in the top right of the window (in the titlebar) that can be used to wake it back up.

The floating point emulation code doesn't work well -- it works well enough that Newton can progress past coprocessor commands, but the results are nonsensical.  Due to this the calculator application will:

1. Crash the Notepad ROM
2. On all other ROMs, it will report "Number too large" for everything. 

### Diagnostics

All ROM images are able to boot into diagnostics.  This can be done by going to the Hardware menu, and picking Diagnostics Reboot.

## Tested ROM Images

Known to work with the following ROM images:

1. [Notepad v1.0b1](http://www.unna.org/incoming/notepad-1.0b1.rom.gz)
2. [ARMistice J1 Image](http://www.unna.org/incoming/Newt%20J1Armistice%20image.zip)
3. [OMP v1.0](http://www.unna.org/incoming/MessagePad%20100%20v1.00.rom)
4. [OMP v1.3](http://www.unna.org/incoming/MessagePad%20OMP%20v1.3.rom.zip)
5. [MessagePad 110 v1.2](http://www.unna.org/incoming/MessagePad%20110%20v1.2.rom)
6. [MessagePad 120 v1.3](http://www.unna.org/incoming/MessagePad%20120%20v1.3%20(444217).rom)
7. [MessagePad 130 v2.0](http://www.unna.org/incoming/MessagePad%20130%20v2.x%20(525314).rom.zip)
8. [Sharp ExpertPad PI-7000 v1.1](http://www.unna.org/incoming/Sharp%20ExpertPad%20PI-7000%20v1.10.rom)
9. [Motorola Marco v1.3](http://www.unna.org/incoming/Motorola%20Marco%20v1.3%20(444347).rom)

You can dump your own ROM images using [my fork of NewTen](https://github.com/pablomarx/NewTen). 
