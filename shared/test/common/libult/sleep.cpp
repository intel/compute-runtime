/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/sleep.h"

namespace NEO {
template <class T>
void sleep(const T &sleepDuration) {
    // Do not sleep in ULTs
}
template <class T>
void waitOnCondition(std::condition_variable &condition, std::unique_lock<std::mutex> &lock, const T &duration) {
    // Do not wait in ULTs
}

template void sleep<std::chrono::microseconds>(const std::chrono::microseconds &);
template void sleep<std::chrono::milliseconds>(const std::chrono::milliseconds &);
template void sleep<std::chrono::seconds>(const std::chrono::seconds &);
template void waitOnCondition<std::chrono::milliseconds>(std::condition_variable &condition, std::unique_lock<std::mutex> &lock, const std::chrono::milliseconds &duration);
} // namespace NEO
