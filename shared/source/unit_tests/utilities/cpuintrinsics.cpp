/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#include <atomic>
#include <cstdint>

//std::atomic is used for sake of sanitation in MT tests
std::atomic<uintptr_t> lastClFlushedPtr(0u);
std::atomic<uint32_t> pauseCounter(0u);

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
    lastClFlushedPtr = reinterpret_cast<uintptr_t>(ptr);
}

void pause() {
    pauseCounter++;
}

} // namespace CpuIntrinsics
} // namespace NEO
