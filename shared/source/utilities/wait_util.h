/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/cpuintrinsics.h"

#include <cstdint>
#include <functional>
#include <thread>

namespace NEO {

namespace WaitUtils {

constexpr uint32_t defaultWaitCount = 1u;
extern uint32_t waitCount;

template <typename T>
inline bool waitFunctionWithPredicate(volatile T const *pollAddress, T expectedValue, std::function<bool(T, T)> predicate) {
    for (uint32_t i = 0; i < waitCount; i++) {
        CpuIntrinsics::pause();
    }
    if (pollAddress != nullptr) {
        if (predicate(*pollAddress, expectedValue)) {
            return true;
        }
    }
    std::this_thread::yield();
    return false;
}

inline bool waitFunction(volatile uint32_t *pollAddress, uint32_t expectedValue) {
    return waitFunctionWithPredicate<uint32_t>(pollAddress, expectedValue, std::greater_equal<uint32_t>());
}

void init();
} // namespace WaitUtils

} // namespace NEO
