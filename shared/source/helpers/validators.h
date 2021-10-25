/*
 * Copyright (C) 2021 Intel Corporation
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
} // namespace NEO
