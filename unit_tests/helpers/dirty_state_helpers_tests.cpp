/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "core/indirect_heap/indirect_heap.h"
#include "core/memory_manager/graphics_allocation.h"
#include "runtime/helpers/dirty_state_helpers.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

#include <memory>

namespace DirtyStateHelpers {
using namespace NEO;
size_t getSizeInPages(size_t sizeInBytes) {
    return (sizeInBytes + MemoryConstants::pageSize) / MemoryConstants::pageSize;
}

void *buffer = reinterpret_cast<void *>(123);
constexpr size_t bufferSize = 456;

struct HeapDirtyStateTests : ::testing::Test {
    struct MockHeapDirtyState : public HeapDirtyState {
        using HeapDirtyState::gpuBaseAddress;
        using HeapDirtyState::sizeInPages;
    };

    void SetUp() override {
        stream.reset(new IndirectHeap(&heapAllocation));
        ASSERT_EQ(heapAllocation.getUnderlyingBuffer(), stream->getCpuBase());
        ASSERT_EQ(heapAllocation.getUnderlyingBufferSize(), stream->getMaxAvailableSpace());
    }

    MockGraphicsAllocation heapAllocation = {buffer, bufferSize};

    std::unique_ptr<IndirectHeap> stream;
    MockHeapDirtyState mockHeapDirtyState;
};

TEST_F(HeapDirtyStateTests, initialValues) {
    EXPECT_EQ(0u, mockHeapDirtyState.sizeInPages);
    EXPECT_EQ(0llu, mockHeapDirtyState.gpuBaseAddress);
}

TEST_F(HeapDirtyStateTests, givenInitializedObjectWhenUpdatedMultipleTimesThenSetValuesAndReturnDirtyOnce) {
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(getSizeInPages(bufferSize), mockHeapDirtyState.sizeInPages);
    EXPECT_EQ(castToUint64(buffer), mockHeapDirtyState.gpuBaseAddress);

    EXPECT_FALSE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(getSizeInPages(bufferSize), mockHeapDirtyState.sizeInPages);
    EXPECT_EQ(castToUint64(buffer), mockHeapDirtyState.gpuBaseAddress);
}

TEST_F(HeapDirtyStateTests, givenNonDirtyObjectWhenAddressChangedThenReturnDirty) {
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    auto newBuffer = ptrOffset(buffer, MemoryConstants::pageSize + 1);
    auto graphicsAllocation = stream->getGraphicsAllocation();
    graphicsAllocation->setCpuPtrAndGpuAddress(newBuffer, castToUint64(newBuffer));

    stream->replaceBuffer(newBuffer, bufferSize);

    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(1u, mockHeapDirtyState.sizeInPages);
    EXPECT_EQ(castToUint64(newBuffer), mockHeapDirtyState.gpuBaseAddress);
}

TEST_F(HeapDirtyStateTests, givenIndirectHeapWithoutGraphicsAllocationWhenUpdateAndCheckIsCalledThenSizeIsSetToZero) {
    IndirectHeap nullHeap(nullptr);

    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));
    EXPECT_NE(0llu, mockHeapDirtyState.sizeInPages);
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(&nullHeap));
    EXPECT_EQ(0llu, mockHeapDirtyState.sizeInPages);
}

TEST_F(HeapDirtyStateTests, givenNonDirtyObjectWhenSizeChangedThenReturnDirty) {
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    auto newBufferSize = bufferSize + MemoryConstants::pageSize;
    stream->replaceBuffer(stream->getCpuBase(), newBufferSize);

    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(getSizeInPages(newBufferSize), mockHeapDirtyState.sizeInPages);
    EXPECT_EQ(castToUint64(buffer), mockHeapDirtyState.gpuBaseAddress);
}

TEST_F(HeapDirtyStateTests, givenNonDirtyObjectWhenSizeAndBufferChangedThenReturnDirty) {
    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    auto newBuffer = ptrOffset(buffer, 1);
    auto newBufferSize = bufferSize + MemoryConstants::pageSize;
    auto graphicsAllocation = stream->getGraphicsAllocation();
    graphicsAllocation->setSize(newBufferSize);
    graphicsAllocation->setCpuPtrAndGpuAddress(newBuffer, castToUint64(newBuffer));
    stream->replaceBuffer(stream->getCpuBase(), newBufferSize);

    EXPECT_TRUE(mockHeapDirtyState.updateAndCheck(stream.get()));

    EXPECT_EQ(getSizeInPages(newBufferSize), mockHeapDirtyState.sizeInPages);
    EXPECT_EQ(castToUint64(newBuffer), mockHeapDirtyState.gpuBaseAddress);
}

TEST(DirtyStateHelpers, givenDirtyStateHelperWhenTwoDifferentIndirectHeapsAreCheckedButWithTheSame4GBbaseThenStateIsNotDirty) {
    MockGraphicsAllocation firstHeapAllocation(reinterpret_cast<void *>(0x1234), 4192);
    MockGraphicsAllocation secondHeapAllocation(reinterpret_cast<void *>(0x9345), 1234);
    uint64_t commonBase = 0x8123456;
    firstHeapAllocation.setGpuBaseAddress(commonBase);
    secondHeapAllocation.setGpuBaseAddress(commonBase);

    IndirectHeap firstHeap(&firstHeapAllocation, true);
    IndirectHeap secondHeap(&secondHeapAllocation, true);

    HeapDirtyState heapChecker;
    auto dirty = heapChecker.updateAndCheck(&firstHeap);
    EXPECT_TRUE(dirty);
    dirty = heapChecker.updateAndCheck(&secondHeap);
    EXPECT_FALSE(dirty);
}

} // namespace DirtyStateHelpers
