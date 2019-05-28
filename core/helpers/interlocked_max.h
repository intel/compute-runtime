/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <atomic>
template <typename Type>
void interlockedMax(std::atomic<Type> &dest, Type newVal) {
    Type oldVal = dest;
    Type maxVal = oldVal < newVal ? newVal : oldVal;
    while (!std::atomic_compare_exchange_weak(&dest, &oldVal, maxVal)) {
        oldVal = dest;
        maxVal = oldVal < newVal ? newVal : oldVal;
    }
}