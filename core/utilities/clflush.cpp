/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/utilities/clflush.h"

#include <emmintrin.h>

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
    _mm_clflush(ptr);
}

} // namespace CpuIntrinsics
} // namespace NEO
