/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/command_stream/command_stream_aub_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

void CommandStreamAubFixture::setUp() {
    AUBFixtureL0::setUp();
    auto streamAllocation = this->allocate(MemoryConstants::pageSize, NEO::AllocationType::commandBuffer);
    ASSERT_NE(nullptr, streamAllocation);
    this->taskStream = std::make_unique<NEO::LinearStream>(streamAllocation);
}

void CommandStreamAubFixture::tearDown() {
    for (auto allocation : this->allocations) {
        this->neoDevice->getMemoryManager()->freeGraphicsMemory(allocation);
    }
    AUBFixtureL0::tearDown();
}

NEO::GraphicsAllocation *CommandStreamAubFixture::allocate(size_t size, NEO::AllocationType allocationType) {
    NEO::AllocationProperties properties{this->neoDevice->getRootDeviceIndex(), true, size, allocationType, false, this->neoDevice->getDeviceBitfield()};
    auto allocation = this->neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    if (allocation) {
        this->allocations.push_back(allocation);
    }
    return allocation;
}

NEO::GraphicsAllocation *CommandStreamAubFixture::createBufferWithData(const void *hostPtr, size_t size) {
    auto allocation = this->allocate(size, NEO::AllocationType::buffer);
    EXPECT_NE(nullptr, allocation);
    if (allocation) {
        EXPECT_TRUE(NEO::MemoryTransferHelper::transferMemoryToAllocation(allocation->isCompressionEnabled(), *this->neoDevice, allocation, 0, hostPtr, size));
    }
    return allocation;
}

void CommandStreamAubFixture::flushStream() {
    NEO::DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.guardCommandBufferWithPipeControl = true;
    this->flushStream(0u, dispatchFlags);
}

void CommandStreamAubFixture::flushStream(size_t streamStartOffset, NEO::DispatchFlags &dispatchFlags) {
    for (auto allocation : this->allocations) {
        this->csr->makeResident(*allocation);
    }

    this->csr->flushTask(*this->taskStream, streamStartOffset,
                         &this->csr->getIndirectHeap(NEO::IndirectHeapType::dynamicState, 0u),
                         &this->csr->getIndirectHeap(NEO::IndirectHeapType::indirectObject, 0u),
                         &this->csr->getIndirectHeap(NEO::IndirectHeapType::surfaceState, 0u),
                         0u, dispatchFlags, *this->neoDevice);
}

} // namespace ult
} // namespace L0
