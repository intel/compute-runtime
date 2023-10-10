/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <condition_variable>
#include <thread>

namespace NEO {
template <class T>
void sleep(const T &sleepDuration);

template <class T>
void waitOnCondition(std::condition_variable &condition, std::unique_lock<std::mutex> &lock, const T &duration);
} // namespace NEO
