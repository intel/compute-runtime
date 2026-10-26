/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/memory_manager/residency_container.h"

#include <vector>

namespace NEO {
using CmdBufferContainer = std::vector<GraphicsAllocation *>;
using HeapContainer = std::vector<GraphicsAllocation *>;
using HeapType = IndirectHeapType;

namespace CSRequirements {
// cleanup section usually contains 1-2 pipeControls BB end and place for BB start
// that makes 16 * 2 + 4 + 8 = 40 bytes
// then command buffer is aligned to cacheline that can take up to 63 bytes
// to be sure everything fits minimal size is at 2 x cacheline.

inline constexpr auto minCommandQueueCommandStreamSize = 2 * MemoryConstants::cacheLineSize;
inline constexpr auto csOverfetchSize = MemoryConstants::pageSize;
} // namespace CSRequirements

} // namespace NEO
