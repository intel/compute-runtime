/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <string>

namespace L0 {
namespace Sysman {

struct DrmRasNode {
    uint32_t nodeId;
    std::string deviceName;
    std::string nodeName;
    uint32_t nodeType;
};

struct DrmErrorCounter {
    uint32_t nodeId = 0;
    std::string errorName = {};
    uint32_t errorValue = 0;
    uint32_t errorId = 0;
};

} // namespace Sysman
} // namespace L0
