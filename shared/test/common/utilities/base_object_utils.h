/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>

namespace NEO {

template <typename T>
struct ReleaseObject {
    void operator()(T *t) {
        if (t != nullptr) {
            t->release();
        }
    }
};

template <typename T>
using ReleaseableObjectPtr = std::unique_ptr<T, ReleaseObject<T>>;

template <typename T>
static ReleaseableObjectPtr<T> clUniquePtr(T *object) {
    return ReleaseableObjectPtr<T>{object};
}

template <class Type, class... Types>
inline ReleaseableObjectPtr<Type> makeReleaseable(Types &&...args) {
    return (ReleaseableObjectPtr<Type>(new Type(std::forward<Types>(args)...)));
}
} // namespace NEO
