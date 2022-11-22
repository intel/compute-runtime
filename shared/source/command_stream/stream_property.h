/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <stddef.h>

namespace NEO {

template <typename Type>
struct StreamPropertyType {
    static constexpr Type initValue = static_cast<Type>(-1);

    Type value = initValue;
    bool isDirty = false;
    void set(Type newValue) {
        if ((value != newValue) && (newValue != initValue)) {
            value = newValue;
            isDirty = true;
        }
    }
};

using StreamProperty32 = StreamPropertyType<int32_t>;
using StreamProperty64 = StreamPropertyType<int64_t>;
using StreamPropertySizeT = StreamPropertyType<size_t>;

using StreamProperty = StreamProperty32;

} // namespace NEO
