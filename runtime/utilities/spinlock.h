/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <atomic>

namespace OCLRT {

class SpinLock {
  public:
    void enter(std::atomic_flag &spinLock) {
        while (spinLock.test_and_set(std::memory_order_acquire)) {
        };
    }
    void leave(std::atomic_flag &spinLock) {
        spinLock.clear(std::memory_order_release);
    }
};

} // namespace OCLRT
