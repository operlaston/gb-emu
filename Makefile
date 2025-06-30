CC = g++
CCFLAGS = -g -Wall -Wextra -std=c++17 -O2 -I/usr/local/include
LDFLAGS = -L/usr/local/lib -lSDL2
OBJ = main.o gameboy.o cpu.o cpu_table.o memory.o gpu.o timer.o joypad.o
TARGET = gameboy

gameboy: $(OBJ)
	$(CC) $(CCFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

main.o: main.cc
	$(CC) $(CCFLAGS) -c main.cc

gameboy.o: gameboy.cc
	$(CC) $(CCFLAGS) -c gameboy.cc

cpu.o: cpu.cc 
	$(CC) $(CCFLAGS) -c cpu.cc

cpu_table.o: cpu_table.cc
	$(CC) $(CCFLAGS) -c cpu_table.cc

memory.o: memory.cc
	$(CC) $(CCFLAGS) -c memory.cc

gpu.o: gpu.cc
	$(CC) $(CCFLAGS) -c gpu.cc

timer.o: timer.cc
	$(CC) $(CCFLAGS) -c timer.cc

joypad.o: joypad.cc
	$(CC) $(CCFLAGS) -c joypad.cc

clean:
	rm -f *.o $(TARGET)
