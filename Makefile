CC = g++
CCFLAGS = -g -Wall -Wextra -std=c++17 -O2
OBJ = gameboy.o cpu.o cpu_table.o memory.o main.o
TARGET = gameboy

gameboy: gameboy.o cpu.o cpu_table.o memory.o main.o
	$(CC) $(CCFLAGS) -o $(TARGET) $(OBJ)

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

clean:
	rm -f *.o $(TARGET)
