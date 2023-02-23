/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
struct HardwareIpVersion {
    HardwareIpVersion() = default;
    HardwareIpVersion(uint32_t ipVersion) : value(ipVersion){};
    union {
        uint32_t value;
        struct
        {
            uint32_t revision : 6;
            uint32_t reserved : 8;
            uint32_t release : 8;
            uint32_t architecture : 10;
        };
    };
};
} // namespace NEO
