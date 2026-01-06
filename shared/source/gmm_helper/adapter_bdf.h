/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

struct AdapterBdf {
    union {
        struct {
            uint32_t bus : 8;
            uint32_t device : 8;
            uint32_t function : 8;
            uint32_t reserved : 8;
        };
        uint32_t data;
    };
};

} // namespace NEO
