/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#include <emmintrin.h>

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
