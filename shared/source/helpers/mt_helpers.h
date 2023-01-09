/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <atomic>

namespace NEO {
namespace MultiThreadHelpers {

template <typename Type>
inline bool atomicCompareExchangeWeakSpin(std::atomic<Type> &destination,
                                          Type &expectedValue,
                                          const Type desiredValue) {
    const Type currentValue = destination;
    if (currentValue == expectedValue) {
        if (destination.compare_exchange_weak(expectedValue, desiredValue)) {
            return true;
        }
    } else {
        expectedValue = currentValue;
    }
    return false;
}

template <typename Type>
void interlockedMax(std::atomic<Type> &dest, Type newVal) {
    Type oldVal = dest;
    Type maxVal = oldVal < newVal ? newVal : oldVal;
    while (!atomicCompareExchangeWeakSpin(dest, oldVal, maxVal)) {
        maxVal = oldVal < newVal ? newVal : oldVal;
    }
}
} // namespace MultiThreadHelpers
} // namespace NEO