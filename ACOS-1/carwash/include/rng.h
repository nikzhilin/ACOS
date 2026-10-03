#ifndef RNG_H
#define RNG_H

// Свой xorshift32 вместо rand(): с одним seed прогон одинаковый и на Linux, и на macOS

#include <stdint.h>

void RngSeed(uint32_t seed);
int RngRange(int lo, int hi);  // [lo, hi]
int RngChance(double p);

#endif
