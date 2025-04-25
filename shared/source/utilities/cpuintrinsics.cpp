/*
 * Copyright (C) 2020-2025 Intel Corporation
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
#if defined(__ARM_ARCH)
extern "C" uint64_t __rdtsc();
#else
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

void mfence() {
    _mm_mfence();
}

void pause() {
    _mm_pause();
}

uint8_t tpause(uint32_t control, uint64_t counter) {
#ifdef SUPPORTS_WAITPKG
    return _tpause(control, counter);
#else
    return 0;
#endif
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

uint64_t rdtsc() {
    return __rdtsc();
}

} // namespace CpuIntrinsics
} // namespace NEO
