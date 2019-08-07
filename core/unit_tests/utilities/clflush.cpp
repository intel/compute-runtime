/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/utilities/clflush.h"

void const *lastClFlushedPtr = nullptr;

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
    lastClFlushedPtr = ptr;
}

} // namespace CpuIntrinsics
} // namespace NEO
