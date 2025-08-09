## gb-emu
Gameboy Emulator written in C++ which supports MBC1 and MBC3.

## Dependencies
SDL2, GNU make, gcc

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
