#ifndef GPU_H
#define GPU_H

#include "memory.hh"

class Gpu {
  Memory& mmu;
public:
  Gpu(Memory& mem);
  ~Gpu() = default;
};

#endif
