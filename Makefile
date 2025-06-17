CC = g++
CCFLAGS = -g -Wall -Wextra -std=c++17 -O2
OBJ = cpu.o main.o
TARGET = gameboy

gameboy: cpu.o main.o
	$(CC) $(CCFLAGS) -o $(TARGET) $(OBJ)

cpu.o: cpu.cc cpu.hh
	$(CC) $(CCFLAGS) -c cpu.cc

main.o: main.cc cpu.hh
	$(CC) $(CCFLAGS) -c main.cc

clean:
	rm -f *.o $(TARGET)
