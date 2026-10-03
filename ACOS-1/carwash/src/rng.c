#include "rng.h"

// xorshift не выходит из нуля, поэтому ноль как seed заменяем
static uint32_t state = 2463534242U;

void RngSeed(uint32_t seed) {
    state = seed ? seed : 2463534242U;
}

// Marsaglia, "Xorshift RNGs", 2003
static uint32_t Next(void) {
    state ^= state << 13U;
    state ^= state >> 17U;
    state ^= state << 5U;
    return state;
}

int RngRange(int lo, int hi) {
    if (hi <= lo) {
        return lo;
    }
    return lo + (int)(Next() % (uint32_t)(hi - lo + 1));
}

int RngChance(double p) {
    return (Next() / 4294967296.0) < p;
}
