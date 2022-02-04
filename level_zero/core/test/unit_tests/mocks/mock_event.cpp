/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

namespace L0 {
namespace ult {

Mock<Event>::Mock() : mockAllocation(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                     &memory, reinterpret_cast<uint64_t>(&memory), 0, sizeof(memory),
                                     MemoryPool::System4KBPages) {}

Mock<Event>::~Mock() {}

} // namespace ult
} // namespace L0
