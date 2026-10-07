/**
 * @file rng.h
 * @brief Генератор случайных чисел xorshift32.
 *
 * Свой генератор вместо rand(): с одним seed прогон одинаковый и на Linux, и на macOS.
 */
#ifndef RNG_H
#define RNG_H

#include <stdint.h>

void RngSeed(uint32_t seed);
int RngRange(int lo, int hi);  ///< случайное число из [lo, hi]
int RngChance(double p);       ///< 1 с вероятностью p

#endif
