/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <thread>

namespace NEO {
template <class T>
void sleep(const T &sleepDuration) {
    // Do not sleep in ULTs
}

template void sleep<std::chrono::microseconds>(const std::chrono::microseconds &);
template void sleep<std::chrono::milliseconds>(const std::chrono::milliseconds &);
template void sleep<std::chrono::seconds>(const std::chrono::seconds &);
} // namespace NEO
