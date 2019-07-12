/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/non_copyable_or_moveable.h"

#include <atomic>

namespace NEO {

class SpinLock : NonCopyableOrMovableClass {
  public:
    SpinLock() = default;
    ~SpinLock() = default;

    void lock() {
        while (flag.test_and_set(std::memory_order_acquire))
            ;
    }

    bool try_lock() { // NOLINT
        return flag.test_and_set(std::memory_order_acquire) == false;
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }

  protected:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

} // namespace NEO
