/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/cpuintrinsics.h"

#include <cstdint>
#include <thread>

namespace NEO {

namespace WaitUtils {

constexpr uint32_t defaultWaitCount = 1u;
extern uint32_t waitCount;

inline bool waitFunction(volatile uint32_t *pollAddress, uint32_t expectedValue) {
    for (uint32_t i = 0; i < waitCount; i++) {
        CpuIntrinsics::pause();
    }
    if (pollAddress != nullptr) {
        if (*pollAddress >= expectedValue) {
            return true;
        }
    }
    std::this_thread::yield();
    return false;
}

void init();
} // namespace WaitUtils

} // namespace NEO
