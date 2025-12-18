/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include <cstdint>
#include <utility>

namespace NEO {
class GraphicsAllocation;

using PrivateAllocsToReuseContainer = StackVec<std::pair<uint32_t, GraphicsAllocation *>, 8>;

} // namespace NEO
