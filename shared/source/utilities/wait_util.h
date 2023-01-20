/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
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

inline bool waitFunction(volatile TagAddressType *pollAddress, TaskCountType expectedValue) {
    return waitFunctionWithPredicate<TaskCountType>(pollAddress, expectedValue, std::greater_equal<TaskCountType>());
}

void init();
} // namespace WaitUtils

} // namespace NEO
