/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <utility>

namespace NEO {
template <typename T = void>
bool areNotNullptr() {
    return true;
}

template <typename T, typename... RT>
bool areNotNullptr(T t, RT... rt) {
    return (t != nullptr) && areNotNullptr<RT...>(rt...);
}

template <typename T, typename... RT>
bool isAnyNullptr(T t, RT... rt) {
    return !areNotNullptr(t, rt...);
}

template <typename T>
T getIfValid(T value, T defaultValue) {
    return value ? value : defaultValue;
}
} // namespace NEO
