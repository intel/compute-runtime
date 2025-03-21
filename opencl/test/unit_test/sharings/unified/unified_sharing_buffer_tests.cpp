/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/unified/unified_buffer.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/sharings/unified/unified_sharing_fixtures.h"
#include "opencl/test/unit_test/sharings/unified/unified_sharing_mocks.h"

using namespace NEO;

using UnifiedSharingBufferTestsWithMemoryManager = UnifiedSharingFixture<true, true>;
using UnifiedSharingBufferTestsWithInvalidMemoryManager = UnifiedSharingFixture<true, false>;

TEST_F(UnifiedSharingBufferTestsWithMemoryManager, givenUnifiedBufferThenItCanBeAcquiredAndReleased) {
    cl_mem_flags flags{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::win32Nt;
    cl_int retVal{};
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    UnifiedSharingFunctions sharingFunctions;
    MockUnifiedBuffer *sharingHandler = new MockUnifiedBuffer(&sharingFunctions, desc.type);
    buffer->setSharingHandler(sharingHandler);

    ASSERT_EQ(0u, sharingHandler->acquireCount);

    ASSERT_EQ(CL_SUCCESS, sharingHandler->acquire(buffer.get(), context->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(1u, sharingHandler->acquireCount);

    ASSERT_EQ(CL_SUCCESS, sharingHandler->acquire(buffer.get(), context->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(2u, sharingHandler->acquireCount);

    sharingHandler->release(buffer.get(), context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(1u, sharingHandler->acquireCount);

    sharingHandler->release(buffer.get(), context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(0u, sharingHandler->acquireCount);
}

TEST_F(UnifiedSharingBufferTestsWithInvalidMemoryManager, givenValidContextAndAllocationFailsWhenCreatingBufferFromSharedHandleThenReturnInvalidMemObject) {
    cl_mem_flags flags{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::win32Nt;
    cl_int retVal{};
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    ASSERT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(UnifiedSharingBufferTestsWithMemoryManager, givenUnsupportedHandleTypeWhenCreatingBufferFromSharedHandleThenReturnInvalidMemObject) {
    cl_mem_flags flags{};
    cl_int retVal{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);

    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(UnifiedSharingBufferTestsWithMemoryManager, givenValidContextAndMemoryManagerWhenCreatingBufferFromSharedHandleThenReturnSuccess) {
    cl_mem_flags flags{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::win32Nt;
    cl_int retVal{};
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
}

using UnifiedSharingBufferTestsWithMultiRootDevice = MultiRootDeviceFixture;

TEST_F(UnifiedSharingBufferTestsWithMultiRootDevice, givenValidContextWithMultiRootDeviceWhenCreatingBufferFromSharedHandleThenGraphicsAllocationsAreCorrectlySet) {
    cl_mem_flags flags{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::win32Nt;
    cl_int retVal{};
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation(device1->getRootDeviceIndex()));
    EXPECT_NE(nullptr, buffer->getGraphicsAllocation(device2->getRootDeviceIndex()));
    ASSERT_EQ(CL_SUCCESS, retVal);
}