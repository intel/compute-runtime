/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

struct StreamProperty {
    int32_t value = -1;
    bool isDirty = false;
    void set(int32_t newValue) {
        if ((value != newValue) && (newValue != -1)) {
            value = newValue;
            isDirty = true;
        }
    }
};

} // namespace NEO
