/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#if defined(_WIN32)
#include <intrin.h>
#else
#include <x86intrin.h>
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

} // namespace CpuIntrinsics
} // namespace NEO
