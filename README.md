## gb-emu
Gameboy Emulator written in C++ which supports MBC1 and MBC3 (no timer). It is NOT T-Cycle accurate. The timings for DMA transfers, Pixel FIFO operations, and memory r/w during instructions are not accounted for. Audio has not yet been implemented.

## Dependencies
SDL2, GNU make

## Build
Run ```make``` from the project root directory.

## Run
Usage: ```./gameboy [path/to/rom]```<br>
Example: ```./gameboy ~/Downloads/pokemon-blue.gb```

## Keybinds

### Main
A: ```X```<br>
B: ```Z```<br>
Start: ```Enter```<br>
Select: ```Tab```<br>
Up: ```↑```<br>
Down: ```↓```<br>
Left: ```←```<br>
Right: ```→```

### Special
Cycle Speed: ```C```<br>
*Note: Cycle Speed will double the speed of the emulator until reaching 4x speed. If ```C``` is pressed while the emulator is at 4x speed, the emulator will return to normal speed (59.7275hz)*
