/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#if defined(_WIN32)
#include <immintrin.h>
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#else
#if defined(__x86_64__)
#include <immintrin.h>
#include <x86intrin.h>
#endif
#endif

#if defined(__ARM_ARCH)
#include <sse2neon.h>
#else
#include <emmintrin.h>
#endif

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
    _mm_clflush(ptr);
}

void clFlushOpt(void *ptr) {
#ifdef SUPPORTS_CLFLUSHOPT
    _mm_clflushopt(ptr);
#else
    _mm_clflush(ptr);
#endif
}

void sfence() {
    _mm_sfence();
}

void pause() {
    _mm_pause();
}

unsigned char umwait(unsigned int ctrl, uint64_t counter) {
#ifdef SUPPORTS_WAITPKG
    return _umwait(ctrl, counter);
#else
    return 0;
#endif
}

void umonitor(void *a) {
#ifdef SUPPORTS_WAITPKG
    _umonitor(a);
#endif
}

#ifdef __ARM_ARCH_ISA_A64
// Adapted from: https://github.com/cloudius-systems/osv/blob/master/arch/aarch64/arm-clock.cc
uint64_t rdtsc() {
    uint64_t cntvct;
    asm volatile ("mrs %0, cntvct_el0; " : "=r"(cntvct) :: "memory");
    return cntvct;
}

uint64_t rdtsc_barrier() {
    uint64_t cntvct;
    asm volatile ("isb; mrs %0, cntvct_el0; isb; " : "=r"(cntvct) :: "memory");
    return cntvct;
}

uint32_t rdtsc_freq() {
    uint32_t freq_hz;
    asm volatile ("mrs %0, cntfrq_el0; isb; " : "=r"(freq_hz) :: "memory");
    return freq_hz;
}
#else
uint64_t rdtsc(){ return __rdtsc(); }
#endif

} // namespace CpuIntrinsics
} // namespace NEO
