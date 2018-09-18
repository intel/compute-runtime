/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <vector>
#include <utility>

namespace OCLRT {
class GraphicsAllocation;
using ResidencyContainer = std::vector<GraphicsAllocation *>;
using AllocationView = std::pair<uint64_t /*address*/, size_t /*size*/>;

} // namespace OCLRT
