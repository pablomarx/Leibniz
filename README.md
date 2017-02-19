# Leibniz
Work in progress emulator for RUNT based Newtons (OMP, Expert Pad, etc)

## Download

### Mac OS X
Alpha 1 can be downloaded from https://www.dropbox.com/s/uvltellluemz70e/Leibniz%20%28Alpha%201%29.zip?dl=0

Requires an Intel Mac running Mac OS X 10.6 or above.

### UNIX
No pre-built binaries are offered.  You should be able to clone this repo and:

1. cd emu-core
2. make sdlnewton
3. ./sdlnewton &lt;path to rom&gt;


## Status

### NewtonOS 

All ROM images are able to boot into NewtonOS.

All ROM images except for the ARMistice and Notepad will put the device to sleep.  When this happens the screen will dim.  There's a power switch in the top right of the window (in the titlebar) that can be used to wake it back up.

The floating point emulation code doesn't work well -- it works well enough that Newton can progress past coprocessor commands, but the results are nonsensical.  Due to this the calculator application will:

1. Crash the Notepad ROM
2. On all other ROMs, it will report "Number too large" for everything. 

Text input in the listener windows ("&lt;stdin&gt;", "WallyScript Listener") is only working under the ARMistice and MP130 v2.0 ROM images.

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

## Screenshots

The ARMistice J1 Image:

![image](https://i.imgur.com/0xqhOgS.png)

For comparison, the same ROM image in it's original environment (a Macintosh IIci with [ARMistice NuBus board](https://www.flickr.com/photos/pablo_marx/4683061782) running the Egger software)

![image](https://i.imgur.com/2zTGOB5.jpg)


The Notepad 1.0b1 image running in SDL on Ubuntu

![image](https://i.imgur.com/ieMcCnl.png)
