# Leibniz
Work in progress emulator for RUNT based Newtons (OMP, Expert Pad, etc)

## Download

### Mac OS X
Pre-built binaries can be downloaded from [the releases page](https://github.com/pablomarx/Leibniz/releases)

Requires an Intel or Apple Silicon Mac running Mac OS X 10.6 or above.

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

1. [Notepad v1.0b1](https://archive.org/download/AppleNewtonROMs/Notepad%20v1.0b1.rom)
2. [ARMistice J1 Image](https://archive.org/download/AppleNewtonROMs/Newt%20J1Armistice%20image)
3. [OMP v1.0](https://archive.org/download/AppleNewtonROMs/MessagePad%20OMP%20v1.00.rom)
4. [Siemens NotePhone v1.1](https://archive.org/download/AppleNewtonROMs/Siemens%20NotePhone%20v1.1.rom)
5. [OMP v1.3](https://archive.org/download/AppleNewtonROMs/MessagePad%20OMP%20v1.3.rom)
6. [MessagePad 110 v1.2](https://archive.org/download/AppleNewtonROMs/MessagePad%20110%20v1.2.rom)
7. [MessagePad 120 v1.3](https://archive.org/download/AppleNewtonROMs/MessagePad%20120%20v1.3%20%28444217%29.rom)
8. [MessagePad 130 v2.0](https://archive.org/download/AppleNewtonROMs/MessagePad%20130%20v2.x%20%28525314%29.rom)
9. [Lindy 803AS.00 (2.0a6)](https://archive.org/download/AppleNewtonROMs/Lindy%20803AS.00%20%282.0a6%29.rom)
10. [Sharp ExpertPad PI-7000 v1.1](https://archive.org/download/AppleNewtonROMs/Sharp%20ExpertPad%20PI-7000%20v1.10.rom)
11. [Motorola Marco v1.3](https://archive.org/download/AppleNewtonROMs/Motorola%20Marco%20v1.3%20%28444347%29.rom)

You can dump your own ROM images using [my fork of NewTen](https://github.com/pablomarx/NewTen). 

## Screenshots

The ARMistice J1 Image:

![image](https://i.imgur.com/0xqhOgS.png)

For comparison, the same ROM image in it's original environment (a Macintosh IIci with [ARMistice NuBus board](https://www.flickr.com/photos/pablo_marx/4683061782) running the Egger software)

![image](https://i.imgur.com/2zTGOB5.jpg)


The Notepad 1.0b1 image running in SDL on Ubuntu:

![image](https://i.imgur.com/ieMcCnl.png)

The same, on a Raspberry Pi 3 running Raspbian (execution speed could be considered 'glacial'):

![image](http://i.imgur.com/7iBHnkp.png)
