/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_output_guard.h"

#include <condition_variable>
#include <cstdio>
#include <mutex>

namespace UltOutputGuard {
namespace {
std::atomic<bool> busy = false;
std::mutex drainMutex;
std::condition_variable drained;

bool outputPending() {
    return pipesHaveUnreadData() || busy;
}
} // namespace

void markBusy() {
    busy = true;
}

void markIdle() {
    {
        std::lock_guard<std::mutex> lock(drainMutex);
        busy = false;
    }
    drained.notify_all();
}

void drain() {
    fflush(stdout);
    fflush(stderr);
    if (!outputPending()) {
        return;
    }
    std::unique_lock<std::mutex> lock(drainMutex);
    drained.wait(lock, [] { return !outputPending(); });
}

} // namespace UltOutputGuard
