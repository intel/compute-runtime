/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/unified/unified_buffer.h"
#include "unit_tests/sharings/unified/unified_sharing_fixtures.h"

using UnifiedSharingBufferTestsWithMemoryManager = UnifiedSharingFixture<true, true>;
using UnifiedSharingBufferTestsWithInvalidMemoryManager = UnifiedSharingFixture<true, false>;

TEST_F(UnifiedSharingBufferTestsWithMemoryManager, givenUnifiedBufferThenItCanBeAcquiredAndReleased) {
    struct MockSharingHandler : UnifiedBuffer {
        using UnifiedBuffer::acquireCount;
        using UnifiedBuffer::UnifiedBuffer;
    };

    cl_mem_flags flags{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    cl_int retVal{};
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    UnifiedSharingFunctions sharingFunctions;
    MockSharingHandler *sharingHandler = new MockSharingHandler(&sharingFunctions, UnifiedSharingHandleType::Win32Nt);
    buffer->setSharingHandler(sharingHandler);

    ASSERT_EQ(0u, sharingHandler->acquireCount);

    ASSERT_EQ(CL_SUCCESS, sharingHandler->acquire(buffer.get()));
    EXPECT_EQ(1u, sharingHandler->acquireCount);

    ASSERT_EQ(CL_SUCCESS, sharingHandler->acquire(buffer.get()));
    EXPECT_EQ(2u, sharingHandler->acquireCount);

    sharingHandler->release(buffer.get());
    EXPECT_EQ(1u, sharingHandler->acquireCount);

    sharingHandler->release(buffer.get());
    EXPECT_EQ(0u, sharingHandler->acquireCount);
}

TEST_F(UnifiedSharingBufferTestsWithInvalidMemoryManager, givenValidContextAndAllocationFailsWhenCreatingBufferFromSharedHandleThenReturnInvalidMemObject) {
    cl_mem_flags flags{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    cl_int retVal{};
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    ASSERT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}
