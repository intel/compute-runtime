/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_memory_manager.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/string.h"

#include <iostream>

namespace L0 {
namespace ult {

using namespace testing;
using ::testing::Return;

Mock<NEO::MemoryManager>::Mock(NEO::ExecutionEnvironment &executionEnvironment) : MemoryManagerMock(executionEnvironment) {
    EXPECT_CALL(*this, allocateGraphicsMemoryInPreferredPool)
        .WillRepeatedly(Invoke(this, &Mock<NEO::MemoryManager>::doAllocateGraphicsMemoryInPreferredPool));
    EXPECT_CALL(*this, freeGraphicsMemory)
        .WillRepeatedly(Invoke(this, &Mock<NEO::MemoryManager>::doFreeGraphicsMemory));
}

inline NEO::GraphicsAllocation *Mock<NEO::MemoryManager>::doAllocateGraphicsMemoryInPreferredPool(const NEO::AllocationProperties &properties, const void *hostPtr) {
    void *buffer;
    if (hostPtr != nullptr) {
        buffer = const_cast<void *>(hostPtr);
    } else {
        buffer = alignedMalloc(properties.size, MemoryConstants::pageSize);
    }
    uint64_t baseAddress = 0;
    if (properties.allocationType == NEO::GraphicsAllocation::AllocationType::INTERNAL_HEAP) {
        baseAddress = castToUint64(alignDown(buffer, MemoryConstants::pageSize64k));
    }
    auto allocation = new NEO::GraphicsAllocation(properties.rootDeviceIndex, properties.allocationType,
                                                  buffer, reinterpret_cast<uint64_t>(buffer), baseAddress, properties.size,
                                                  MemoryPool::System4KBPages);

    return allocation;
}

inline void Mock<NEO::MemoryManager>::doFreeGraphicsMemory(NEO::GraphicsAllocation *allocation) {
    if (allocation == nullptr) {
        return;
    }
    alignedFree(allocation->getUnderlyingBuffer());
    delete allocation;
}

} // namespace ult
} // namespace L0
