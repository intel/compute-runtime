/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>

namespace NEO {

template <typename T>
struct DestroyObject {
    void operator()(T *obj) {
        if (obj) {
            obj->destroy();
        }
    }
};

template <typename T>
using DestroyableZeUniquePtr = std::unique_ptr<T, DestroyObject<T>>;

template <typename T>
static DestroyableZeUniquePtr<T> zeUniquePtr(T *object) {
    return DestroyableZeUniquePtr<T>{object};
}

template <class _Ty, class... _Types>
inline DestroyableZeUniquePtr<_Ty> makeZeUniquePtr(_Types &&...args) {
    return (DestroyableZeUniquePtr<_Ty>(new _Ty(std::forward<_Types>(args)...)));
}
} // namespace NEO
