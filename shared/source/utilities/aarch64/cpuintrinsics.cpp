/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"
#include <sse2neon.h>

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
    return 0;
}

} // namespace CpuIntrinsics
} // namespace NEO
