/*
 * Copyright (C) 2021-2026 Intel Corporation
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
#include <utility>

namespace NEO {

struct HardwareInfo;

namespace WaitUtils {

enum class WaitpkgUse : int32_t {
    uninitialized = -1,
    noUse = 0,
    umonitorAndUmwait,
    tpause
};

constexpr int64_t defaultWaitPkgThresholdInMicroSeconds = 20;
constexpr int64_t defaultWaitPkgThresholdForDiscreteInMicroSeconds = 28;
constexpr uint64_t defaultCounterValue = 12000;
constexpr uint32_t defaultControlValue = 0;
constexpr uint32_t defaultWaitCount = 1u;

constexpr int64_t defaultWaitPkgThresholdForUllsLightInMicroSeconds = 1;
constexpr uint64_t defaultCounterValueForUllsLight = 16000;

constexpr int64_t defaultWaitPkgThresholdForEventHostSyncInMicroSeconds = 50;
constexpr uint64_t defaultCounterValueForEventHostSync = 2000;

extern WaitpkgUse waitpkgUse;
extern int64_t waitPkgThresholdInMicroSeconds;
extern int64_t waitPkgThresholdForEventHostSyncInMicroSeconds;
extern uint64_t waitpkgCounterValue;
extern uint64_t counterValueForEventHostSync;
extern uint32_t waitpkgControlValue;
extern uint32_t waitCount;
extern bool waitpkgSupport;

inline void tpause(uint64_t counterValue) {
    uint64_t currentCounter = CpuIntrinsics::rdtsc() + counterValue;
    CpuIntrinsics::tpause(waitpkgControlValue, currentCounter);
}

inline void tpause() {
    tpause(waitpkgCounterValue);
}

inline bool monitorWait(volatile void const *monitorAddress) {
    uint64_t currentCounter = CpuIntrinsics::rdtsc() + (waitpkgCounterValue);
    CpuIntrinsics::umonitor(const_cast<void *>(monitorAddress));
    return CpuIntrinsics::umwait(waitpkgControlValue, currentCounter) == 0;
}

template <typename T>
inline bool waitFunctionWithPredicate(volatile T const *pollAddress, T expectedValue, std::function<bool(T, T)> predicate, int64_t timeElapsedSinceWaitStarted, uint64_t counterValue, int64_t waitPkgThreshold) {
    if (waitpkgUse == WaitpkgUse::tpause && timeElapsedSinceWaitStarted > waitPkgThreshold) {
        tpause(counterValue);
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

template <typename T>
inline bool waitFunctionWithPredicate(volatile T const *pollAddress, T expectedValue, std::function<bool(T, T)> predicate, int64_t timeElapsedSinceWaitStarted, uint64_t counterValue) {
    return waitFunctionWithPredicate<T>(pollAddress, expectedValue, std::move(predicate), timeElapsedSinceWaitStarted, counterValue, waitPkgThresholdInMicroSeconds);
}

template <typename T>
inline bool waitFunctionWithPredicate(volatile T const *pollAddress, T expectedValue, std::function<bool(T, T)> predicate, int64_t timeElapsedSinceWaitStarted) {
    return waitFunctionWithPredicate<T>(pollAddress, expectedValue, std::move(predicate), timeElapsedSinceWaitStarted, waitpkgCounterValue);
}

inline void waitFunctionWithoutPredicate(int64_t timeElapsedSinceWaitStarted) {
    if (waitpkgUse == WaitpkgUse::tpause && timeElapsedSinceWaitStarted > waitPkgThresholdInMicroSeconds) {
        tpause();
    } else {
        for (uint32_t i = 0; i < waitCount; i++) {
            CpuIntrinsics::pause();
        }
    }
}

inline bool waitFunction(volatile TagAddressType *pollAddress, TaskCountType expectedValue, int64_t timeElapsedSinceWaitStarted) {
    return waitFunctionWithPredicate<TaskCountType>(pollAddress, expectedValue, std::greater_equal<TaskCountType>(), timeElapsedSinceWaitStarted);
}

void init(WaitpkgUse inputWaitpkgUse, const HardwareInfo &hwInfo);
void overrideWaitpkgParams();
void adjustWaitpkgParamsForUllsLight();

} // namespace WaitUtils

} // namespace NEO
