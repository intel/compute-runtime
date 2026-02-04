/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

WhiteBox<::L0::CommandList>::WhiteBox() : BaseClass(BaseClass::defaultNumIddsPerBlock) {}

WhiteBox<::L0::CommandList>::~WhiteBox() {}

Mock<CommandList>::Mock(Device *device) : WhiteBox<::L0::CommandList>() {
    this->device = device;
    size_t batchBufferSize = 65536u;
    batchBuffer = new uint8_t[batchBufferSize];
    mockAllocation = new NEO::GraphicsAllocation(0,
                                                 1u /*num gmms*/,
                                                 NEO::AllocationType::internalHostMemory,
                                                 &batchBuffer,
                                                 reinterpret_cast<uint64_t>(&batchBuffer),
                                                 0,
                                                 sizeof(batchBufferSize),
                                                 NEO::MemoryPool::system4KBPages,
                                                 NEO::MemoryManager::maxOsContextCount);
}

Mock<CommandList>::~Mock() {
    delete mockAllocation;
    delete[] batchBuffer;
}

} // namespace ult
} // namespace L0
