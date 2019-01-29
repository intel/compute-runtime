/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>

namespace OCLRT {

template <typename T>
using ReleaseableObjectPtr = std::unique_ptr<T, void (*)(T *)>;

template <typename T>
static ReleaseableObjectPtr<T> clUniquePtr(T *object) {
    return ReleaseableObjectPtr<T>{object, [](T *p) { p->release(); }};
}
} // namespace OCLRT
