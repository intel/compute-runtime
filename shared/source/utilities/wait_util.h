/*
 * Copyright (C) 2021-2025 Intel Corporation
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

enum class WaitpkgUse : int32_t {
    uninitialized = -1,
    noUse = 0,
    umonitorAndUmwait,
    tpause
};

constexpr int64_t defaultWaitPkgThresholdInMicroSeconds = 12;
constexpr uint64_t defaultCounterValue = 16000;
constexpr uint32_t defaultControlValue = 0;
constexpr uint32_t defaultWaitCount = 1u;

extern WaitpkgUse waitpkgUse;
extern int64_t waitPkgThresholdInMicroSeconds;
extern uint64_t waitpkgCounterValue;
extern uint32_t waitpkgControlValue;
extern uint32_t waitCount;
extern bool waitpkgSupport;

inline void tpause() {
    uint64_t currentCounter = CpuIntrinsics::rdtsc() + waitpkgCounterValue;
    CpuIntrinsics::tpause(waitpkgControlValue, currentCounter);
}

inline bool monitorWait(volatile void const *monitorAddress) {
    uint64_t currentCounter = CpuIntrinsics::rdtsc() + (waitpkgCounterValue);
    CpuIntrinsics::umonitor(const_cast<void *>(monitorAddress));
    return CpuIntrinsics::umwait(waitpkgControlValue, currentCounter) == 0;
}

template <typename T>
inline bool waitFunctionWithPredicate(volatile T const *pollAddress, T expectedValue, std::function<bool(T, T)> predicate, int64_t timeElapsedSinceWaitStarted) {
    if (waitpkgUse == WaitpkgUse::tpause && timeElapsedSinceWaitStarted > waitPkgThresholdInMicroSeconds) {
        tpause();
    } else {
        for (uint32_t i = 0; i < waitCount; i++) {
            CpuIntrinsics::pause();
        }
    }

    if (pollAddress != nullptr) {
        if (predicate(*pollAddress, expectedValue)) {
            return true;
        }
        if (waitpkgUse == WaitpkgUse::umonitorAndUmwait) {
            if (monitorWait(pollAddress)) {
                if (predicate(*pollAddress, expectedValue)) {
                    return true;
                }
            }
        }
    }
    std::this_thread::yield();
    return false;
}

inline bool waitFunction(volatile TagAddressType *pollAddress, TaskCountType expectedValue, int64_t timeElapsedSinceWaitStarted) {
    return waitFunctionWithPredicate<TaskCountType>(pollAddress, expectedValue, std::greater_equal<TaskCountType>(), timeElapsedSinceWaitStarted);
}

void init(WaitpkgUse inputWaitpkgUse);
} // namespace WaitUtils

} // namespace NEO
