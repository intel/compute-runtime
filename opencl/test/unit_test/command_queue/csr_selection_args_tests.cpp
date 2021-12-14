/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/csr_selection_args.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_image.h"

namespace NEO {
TEST(CsrSelectionArgsTests, givenBuffersWhenCreatingCsrSelectionArgsThenSetupArgsCorrectly) {
    const uint32_t rootDeviceIndex = 2u;
    const size_t *size = reinterpret_cast<size_t *>(0x1234);

    MockGraphicsAllocation allocation1{rootDeviceIndex, nullptr, 1024u};
    MockGraphicsAllocation allocation2{rootDeviceIndex, nullptr, 1024u};
    MockBuffer buffer1{allocation1};
    MockBuffer buffer2{allocation2};

    {
        allocation1.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_WRITE_BUFFER, {}, &buffer1, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.dstResource.allocation);
    }
    {
        allocation1.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_WRITE_BUFFER, {}, &buffer1, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToLocal, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.dstResource.allocation);
    }

    {
        allocation1.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_READ_BUFFER, &buffer1, {}, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_BUFFER), args.cmdType);
        EXPECT_EQ(TransferDirection::LocalToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
    }
    {
        allocation1.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_READ_BUFFER, &buffer1, {}, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_BUFFER), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
    }

    {
        allocation1.memoryPool = MemoryPool::LocalMemory;
        allocation2.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &buffer1, &buffer2, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER), args.cmdType);
        EXPECT_EQ(TransferDirection::LocalToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(&allocation2, args.dstResource.allocation);
    }
    {
        allocation1.memoryPool = MemoryPool::System4KBPages;
        allocation2.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_BUFFER, &buffer1, &buffer2, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_BUFFER), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToLocal, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(&allocation2, args.dstResource.allocation);
    }
}

TEST(CsrSelectionArgsTests, givenImagesWhenCreatingCsrSelectionArgsThenSetupArgsCorrectly) {
    const uint32_t rootDeviceIndex = 2u;
    const size_t *size = reinterpret_cast<size_t *>(0x1234);
    const size_t *origin1 = reinterpret_cast<size_t *>(0x12345);
    const size_t *origin2 = reinterpret_cast<size_t *>(0x123456);

    MockImageBase image1{rootDeviceIndex};
    MockImageBase image2{rootDeviceIndex};
    MockGraphicsAllocation &allocation1 = *image1.graphicsAllocation;
    MockGraphicsAllocation &allocation2 = *image2.graphicsAllocation;

    {
        allocation1.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_WRITE_IMAGE, {}, &image1, rootDeviceIndex, size, nullptr, origin1};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_IMAGE), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&image1, args.dstResource.image);
        EXPECT_EQ(&allocation1, args.dstResource.allocation);
        EXPECT_EQ(origin1, args.dstResource.imageOrigin);
    }
    {
        allocation1.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_WRITE_IMAGE, {}, &image1, rootDeviceIndex, size, nullptr, origin1};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_WRITE_IMAGE), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToLocal, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&image1, args.dstResource.image);
        EXPECT_EQ(&allocation1, args.dstResource.allocation);
        EXPECT_EQ(origin1, args.dstResource.imageOrigin);
    }

    {
        allocation1.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_READ_IMAGE, &image1, nullptr, rootDeviceIndex, size, origin1, nullptr};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_IMAGE), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&image1, args.srcResource.image);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(origin1, args.srcResource.imageOrigin);
    }
    {
        allocation1.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_READ_IMAGE, &image1, nullptr, rootDeviceIndex, size, origin1, nullptr};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_READ_IMAGE), args.cmdType);
        EXPECT_EQ(TransferDirection::LocalToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&image1, args.srcResource.image);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(origin1, args.srcResource.imageOrigin);
    }

    {
        allocation1.memoryPool = MemoryPool::System4KBPages;
        allocation2.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_COPY_IMAGE, &image1, &image2, rootDeviceIndex, size, origin1, origin2};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_IMAGE), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToLocal, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&image1, args.srcResource.image);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(origin1, args.srcResource.imageOrigin);
        EXPECT_EQ(&image2, args.dstResource.image);
        EXPECT_EQ(&allocation2, args.dstResource.allocation);
        EXPECT_EQ(origin2, args.dstResource.imageOrigin);
    }
    {
        allocation1.memoryPool = MemoryPool::LocalMemory;
        allocation2.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_COPY_IMAGE, &image1, &image2, rootDeviceIndex, size, origin1, origin2};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_COPY_IMAGE), args.cmdType);
        EXPECT_EQ(TransferDirection::LocalToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&image1, args.srcResource.image);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(origin1, args.srcResource.imageOrigin);
        EXPECT_EQ(&image2, args.dstResource.image);
        EXPECT_EQ(&allocation2, args.dstResource.allocation);
        EXPECT_EQ(origin2, args.dstResource.imageOrigin);
    }
}

TEST(CsrSelectionArgsTests, givenGraphicsAllocationsWhenCreatingCsrSelectionArgsThenSetupArgsCorrectly) {
    const uint32_t rootDeviceIndex = 2u;
    const size_t *size = reinterpret_cast<size_t *>(0x1234);

    MockGraphicsAllocation allocation1{rootDeviceIndex, nullptr, 1024u};
    MockGraphicsAllocation allocation2{rootDeviceIndex, nullptr, 1024u};
    MultiGraphicsAllocation multiAlloc1 = GraphicsAllocationHelper::toMultiGraphicsAllocation(&allocation1);
    MultiGraphicsAllocation multiAlloc2 = GraphicsAllocationHelper::toMultiGraphicsAllocation(&allocation2);

    {
        allocation1.memoryPool = MemoryPool::System4KBPages;
        allocation2.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_SVM_MEMCPY, &multiAlloc1, &multiAlloc2, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(&allocation2, args.dstResource.allocation);
    }
    {
        allocation1.memoryPool = MemoryPool::System4KBPages;
        allocation2.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_SVM_MEMCPY, &multiAlloc1, &multiAlloc2, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY), args.cmdType);
        EXPECT_EQ(TransferDirection::HostToLocal, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(&allocation2, args.dstResource.allocation);
    }
    {
        allocation1.memoryPool = MemoryPool::LocalMemory;
        allocation2.memoryPool = MemoryPool::System4KBPages;
        CsrSelectionArgs args{CL_COMMAND_SVM_MEMCPY, &multiAlloc1, &multiAlloc2, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY), args.cmdType);
        EXPECT_EQ(TransferDirection::LocalToHost, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(&allocation2, args.dstResource.allocation);
    }
    {
        allocation1.memoryPool = MemoryPool::LocalMemory;
        allocation2.memoryPool = MemoryPool::LocalMemory;
        CsrSelectionArgs args{CL_COMMAND_SVM_MEMCPY, &multiAlloc1, &multiAlloc2, rootDeviceIndex, size};
        EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY), args.cmdType);
        EXPECT_EQ(TransferDirection::LocalToLocal, args.direction);
        EXPECT_EQ(size, args.size);
        EXPECT_EQ(&allocation1, args.srcResource.allocation);
        EXPECT_EQ(&allocation2, args.dstResource.allocation);
    }
}

} // namespace NEO
