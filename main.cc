#include <stdio.h>
#include "cpu.hh"

int main() {
  Cpu emu("cpu_instrs.gb");
  while(1){
    emu.fetch_and_execute();
  }
  return 1;
}
