/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
const uint64_t L0::Event::completionTimeoutMs = 1;
namespace ult {

Mock<Event>::Mock() : WhiteBox<::L0::Event>(0, nullptr),
                      mockAllocation(0,
                                     1u /*num gmms*/,
                                     NEO::AllocationType::internalHostMemory,
                                     &memory,
                                     reinterpret_cast<uint64_t>(&memory),
                                     0,
                                     sizeof(memory),
                                     NEO::MemoryPool::system4KBPages,
                                     NEO::MemoryManager::maxOsContextCount) {}

Mock<Event>::~Mock() {}

} // namespace ult
} // namespace L0
