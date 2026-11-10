/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include <functional>

namespace NEO {
class GraphicsAllocation;

using PeerAllocationsFn = std::function<StackVec<GraphicsAllocation *, 4>(const void *)>;
using ReleasePeerAllocationsFn = std::function<void(const void *)>;
} // namespace NEO
