/*
 * Copyright (C) 2022-2025 Intel Corporation
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

template <class T, class Predicate>
bool waitOnConditionWithPredicate(std::condition_variable &condition, std::unique_lock<std::mutex> &lock, const T &duration, Predicate predicate) {
    return condition.wait_for(lock, duration, predicate);
}
} // namespace NEO
