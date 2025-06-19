CC = g++
CCFLAGS = -g -Wall -Wextra -std=c++17 -O2
OBJ = cpu.o cpu_table.o memory.o main.o
TARGET = gameboy

gameboy: cpu.o cpu_table.o memory.o main.o
	$(CC) $(CCFLAGS) -o $(TARGET) $(OBJ)

main.o: main.cc cpu.hh
	$(CC) $(CCFLAGS) -c main.cc

cpu.o: cpu.cc cpu.hh
	$(CC) $(CCFLAGS) -c cpu.cc

cpu_table.o: cpu_table.cc cpu.hh
	$(CC) $(CCFLAGS) -c cpu_table.cc

memory.o: memory.cc memory.hh
	$(CC) $(CCFLAGS) -c memory.cc

clean:
	rm -f *.o $(TARGET)
