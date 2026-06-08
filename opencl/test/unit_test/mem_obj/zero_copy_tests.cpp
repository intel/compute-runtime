/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

class ZeroCopyBufferTest : public ClDeviceFixture,
                           public testing::Test {
  protected:
    void SetUp() override {
        ClDeviceFixture::setUp();
    }

    void TearDown() override {
        ClDeviceFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockContext context;
    cl_mem_flags flags = 0;
    bool shouldBeZeroCopy = false;
    cl_int size = 0;
    bool misalignPointer = false;
};

static const int multiplier = 1000;
static const int cacheLinedAlignedSize = MemoryConstants::cacheLineSize * multiplier;
static const int cacheLinedMisAlignedSize = cacheLinedAlignedSize - 1;
static const int pageAlignSize = MemoryConstants::preferredAlignment * multiplier;

// flags, size to alloc, alignment, size, ZeroCopy, misalignPointer
std::tuple<uint64_t, size_t, size_t, int, bool, bool> inputs[] = {std::make_tuple(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), cacheLinedMisAlignedSize, MemoryConstants::preferredAlignment, cacheLinedMisAlignedSize, false, true),
                                                                  std::make_tuple(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), cacheLinedAlignedSize, MemoryConstants::preferredAlignment, cacheLinedAlignedSize, false, true),
                                                                  std::make_tuple(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), cacheLinedAlignedSize, MemoryConstants::preferredAlignment, cacheLinedAlignedSize, true, false),
                                                                  std::make_tuple(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), cacheLinedMisAlignedSize, MemoryConstants::preferredAlignment, cacheLinedMisAlignedSize, false, false),
                                                                  std::make_tuple(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), pageAlignSize, MemoryConstants::preferredAlignment, pageAlignSize, true, false),
                                                                  std::make_tuple(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), cacheLinedMisAlignedSize, MemoryConstants::cacheLineSize, cacheLinedAlignedSize, true, false),
                                                                  std::make_tuple(static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR), cacheLinedMisAlignedSize, MemoryConstants::preferredAlignment, cacheLinedMisAlignedSize, true, true),
                                                                  std::make_tuple(static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR), cacheLinedMisAlignedSize, MemoryConstants::preferredAlignment, cacheLinedMisAlignedSize, true, false),
                                                                  std::make_tuple(0, 0, 0, cacheLinedMisAlignedSize, true, false),
                                                                  std::make_tuple(0, 0, 0, cacheLinedAlignedSize, true, true)};

TEST_F(ZeroCopyBufferTest, GivenCacheAlignedPointerWhenCreatingBufferThenZeroCopy) {
    void *hostPtr = alignedMalloc(pageAlignSize, MemoryConstants::preferredAlignment);

    for (const auto &input : inputs) {
        size_t sizeToAlloc, alignment;
        std::tie(flags, sizeToAlloc, alignment, size, shouldBeZeroCopy, misalignPointer) = input;

        char *passedPtr = (sizeToAlloc > 0) ? (char *)hostPtr : nullptr;
        if (misalignPointer && passedPtr) {
            passedPtr += 1;
        }

        auto buffer = Buffer::create(&context, flags, size, passedPtr, retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(shouldBeZeroCopy, buffer->isMemObjZeroCopy()) << "Zero Copy not handled properly";
        if (!shouldBeZeroCopy && flags & CL_MEM_USE_HOST_PTR) {
            EXPECT_NE(buffer->getCpuAddress(), hostPtr);
        }
        EXPECT_NE(nullptr, buffer->getCpuAddress());
        EXPECT_EQ(alignUp(buffer->getCpuAddress(), MemoryConstants::cacheLineSize), buffer->getCpuAddress());
        delete buffer;
    }

    alignedFree(hostPtr);
}

TEST(ZeroCopyWithDebugFlag, GivenInputsThatWouldResultInZeroCopyAndUseHostptrDisableZeroCopyFlagWhenBufferIsCreatedThenNonZeroCopyBufferIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DisableZeroCopyForUseHostPtr.set(true);
    MockContext context;
    auto hostPtr = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize;
    auto retVal = CL_SUCCESS;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_USE_HOST_PTR, size, hostPtr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
    alignedFree(hostPtr);
}

TEST(ZeroCopyWithDebugFlag, GivenInputsThatWouldResultInZeroCopyAndDisableZeroCopyFlagWhenBufferIsCreatedThenNonZeroCopyBufferIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DisableZeroCopyForBuffers.set(true);
    MockContext context;
    auto hostPtr = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    auto size = MemoryConstants::pageSize;
    auto retVal = CL_SUCCESS;

    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_USE_HOST_PTR, size, hostPtr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());
    alignedFree(hostPtr);
}

TEST(ZeroCopyWithDebugFlag, GivenBufferInputsThatWouldResultInZeroCopyAndDisableZeroCopyFlagWhenBufferIsCreatedThenNonZeroCopyBufferIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.DisableZeroCopyForBuffers.set(true);
    MockContext context;
    auto retVal = CL_SUCCESS;
    std::unique_ptr<Buffer> buffer(Buffer::create(&context, CL_MEM_ALLOC_HOST_PTR, MemoryConstants::pageSize, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(buffer->isMemObjZeroCopy());
    EXPECT_FALSE(buffer->mappingOnCpuAllowed());
    EXPECT_EQ(nullptr, buffer->getHostPtr());
    EXPECT_EQ(nullptr, buffer->getAllocatedMapPtr());
    auto bufferAllocation = buffer->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getUnderlyingBuffer();

    auto mapAllocation = buffer->getBasePtrForMap(0);
    EXPECT_EQ(mapAllocation, buffer->getAllocatedMapPtr());
    EXPECT_NE(mapAllocation, bufferAllocation);
}
