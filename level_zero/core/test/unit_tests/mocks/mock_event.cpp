/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_event.h"

#include <vector>

namespace L0 {
namespace ult {

Mock<Event>::Mock() : mockAllocation(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                     &memory, reinterpret_cast<uint64_t>(&memory), 0, sizeof(memory),
                                     MemoryPool::System4KBPages) { allocation = &mockAllocation; }

Mock<Event>::~Mock() {}

Mock<EventPool>::Mock() : pool(1) {
    pool = std::vector<int>(1);
    pool[0] = 0;

    EXPECT_CALL(*this, getPoolSize()).WillRepeatedly(testing::Return(1));
}

Mock<EventPool>::~Mock() { pool.clear(); }

} // namespace ult
} // namespace L0
