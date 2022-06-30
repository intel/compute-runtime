/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/cpuintrinsics.h"

#include "shared/source/helpers/ptr_math.h"

#include <atomic>
#include <cstdint>
#include <functional>

namespace CpuIntrinsicsTests {
//std::atomic is used for sake of sanitation in MT tests
std::atomic<uintptr_t> lastClFlushedPtr(0u);
std::atomic<uint32_t> clFlushCounter(0u);
std::atomic<uint32_t> pauseCounter(0u);
std::atomic<uint32_t> sfenceCounter(0u);

volatile uint32_t *pauseAddress = nullptr;
uint32_t pauseValue = 0u;
uint32_t pauseOffset = 0u;

std::function<void()> setupPauseAddress;
} // namespace CpuIntrinsicsTests

namespace NEO {
namespace CpuIntrinsics {

void clFlush(void const *ptr) {
    CpuIntrinsicsTests::clFlushCounter++;
    CpuIntrinsicsTests::lastClFlushedPtr = reinterpret_cast<uintptr_t>(ptr);
}

void sfence() {
    CpuIntrinsicsTests::sfenceCounter++;
}

void pause() {
    CpuIntrinsicsTests::pauseCounter++;
    if (CpuIntrinsicsTests::pauseAddress != nullptr) {
        *CpuIntrinsicsTests::pauseAddress = CpuIntrinsicsTests::pauseValue;
        if (CpuIntrinsicsTests::setupPauseAddress) {
            CpuIntrinsicsTests::setupPauseAddress();
        } else {
            CpuIntrinsicsTests::pauseAddress = ptrOffset(CpuIntrinsicsTests::pauseAddress, CpuIntrinsicsTests::pauseOffset);
        }
    }
}

} // namespace CpuIntrinsics
} // namespace NEO
