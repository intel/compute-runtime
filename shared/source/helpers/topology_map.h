/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace NEO {

struct TopologyMapping {
    std::vector<int> sliceIndices;
};

using TopologyMap = std::unordered_map<uint32_t, TopologyMapping>;

} // namespace NEO
