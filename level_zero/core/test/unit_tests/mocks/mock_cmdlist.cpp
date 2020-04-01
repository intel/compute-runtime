/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_cmdlist.h"

#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

WhiteBox<::L0::CommandList>::WhiteBox(Device *device) {}

WhiteBox<::L0::CommandList>::~WhiteBox() {}

Mock<CommandList>::Mock(Device *device) : WhiteBox<::L0::CommandList>(device) {
    this->device = device;
    size_t batchBufferSize = 65536u;
    batchBuffer = new uint8_t[batchBufferSize];
    mockAllocation = new NEO::GraphicsAllocation(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                 &batchBuffer, reinterpret_cast<uint64_t>(&batchBuffer), 0, sizeof(batchBufferSize),
                                                 MemoryPool::System4KBPages);
}

Mock<CommandList>::~Mock() {
    delete mockAllocation;
    delete[] batchBuffer;
}

} // namespace ult
} // namespace L0
