/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/dirty_state_helpers.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "gtest/gtest.h"
#include <memory>

namespace DirtyStateHelpers {
using namespace OCLRT;
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

    GraphicsAllocation heapAllocation = {buffer, bufferSize};

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
    GraphicsAllocation firstHeapAllocation(reinterpret_cast<void *>(0x1234), 4192);
    GraphicsAllocation secondHeapAllocation(reinterpret_cast<void *>(0x9345), 1234);
    uint64_t commonBase = 0x8123456;
    firstHeapAllocation.gpuBaseAddress = commonBase;
    secondHeapAllocation.gpuBaseAddress = commonBase;

    IndirectHeap firstHeap(&firstHeapAllocation, true);
    IndirectHeap secondHeap(&secondHeapAllocation, true);

    HeapDirtyState heapChecker;
    auto dirty = heapChecker.updateAndCheck(&firstHeap);
    EXPECT_TRUE(dirty);
    dirty = heapChecker.updateAndCheck(&secondHeap);
    EXPECT_FALSE(dirty);
}

} // namespace DirtyStateHelpers
