/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#include <atomic>
#include <cstdint>

namespace CpuIntrinsicsTests {
//std::atomic is used for sake of sanitation in MT tests
std::atomic<uintptr_t> lastClFlushedPtr(0u);
std::atomic<uint32_t> pauseCounter(0u);

volatile uint32_t *pauseAddress = nullptr;
uint32_t pauseValue = 0u;
} // namespace CpuIntrinsicsTests

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
    CpuIntrinsicsTests::lastClFlushedPtr = reinterpret_cast<uintptr_t>(ptr);
}

void pause() {
    CpuIntrinsicsTests::pauseCounter++;
    if (CpuIntrinsicsTests::pauseAddress != nullptr) {
        *CpuIntrinsicsTests::pauseAddress = CpuIntrinsicsTests::pauseValue;
    }
}

} // namespace CpuIntrinsics
} // namespace NEO
