/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

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

void pause() {
    _mm_pause();
}

} // namespace CpuIntrinsics
} // namespace NEO
