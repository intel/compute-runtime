/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <stddef.h>

namespace NEO {

template <typename Type, bool fullStateProperty>
struct StreamPropertyType {
    static constexpr Type initValue = static_cast<Type>(-1);

    Type value = initValue;
    bool isDirty = false;
    void set(Type newValue) {
        if constexpr (fullStateProperty) {
            if ((value != newValue) && (newValue != initValue)) {
                value = newValue;
                isDirty = true;
            }
        } else if (newValue != initValue) {
            value = newValue;
        }
    }
};

using StreamProperty32 = StreamPropertyType<int32_t, true>;
using StreamProperty64 = StreamPropertyType<int64_t, true>;
using StreamPropertySizeT = StreamPropertyType<size_t, false>;

using StreamProperty = StreamProperty32;

} // namespace NEO
