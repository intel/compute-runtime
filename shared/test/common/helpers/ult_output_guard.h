/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <atomic>
#include <cstddef>

namespace UltOutputGuard {

inline std::atomic<size_t> capturedBytes = 0;

void start();
void stop();
void drain();
void onFatal();

bool pipesHaveUnreadData();
void markBusy();
void markIdle();

} // namespace UltOutputGuard
