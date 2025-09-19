/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

using DrmMemoryManagerTest = Test<DrmMemoryManagerFixture>;

struct ClDrmMemoryManagerTest : public DrmMemoryManagerTest {
    template <typename GfxFamily>
    void setUpT() {
        MemoryManagementFixture::setUp();

        executionEnvironment = MockClDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        DrmMemoryManagerFixture::setUpT<GfxFamily>(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]).release(), false);
        pClDevice = new MockClDevice{device}; // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
        device->incRefInternal();
    }

    template <typename GfxFamily>
    void tearDownT() {
        delete pClDevice;
        DrmMemoryManagerFixture::tearDownT<GfxFamily>();
    }

    MockClDevice *pClDevice = nullptr;
};

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferAllocationThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    debugManager.flags.Force32bitAddressing.set(true);
    MockContext context(pClDevice);
    memoryManager->setForce32BitAllocations(true);

    auto size = MemoryConstants::pageSize;
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size,
        nullptr,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    auto bufferAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();
    auto baseAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuBaseAddress();

    EXPECT_LT(ptrDiff(bufferAddress, baseAddress), MemoryConstants::max32BitAddress);

    delete buffer;
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferCreatedFromHostPtrThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    debugManager.flags.Force32bitAddressing.set(true);
    MockContext context(pClDevice);
    memoryManager->setForce32BitAllocations(true);

    auto size = MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<void *>(0x1000);
    auto ptrOffset = MemoryConstants::cacheLineSize;
    uintptr_t offsetPtr = (uintptr_t)ptr + ptrOffset;
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size,
        reinterpret_cast<void *>(offsetPtr),
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(buffer->isMemObjZeroCopy());
    auto bufferAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();
    auto drmAllocation = static_cast<DrmAllocation *>(buffer->getGraphicsAllocation(rootDeviceIndex));

    auto baseAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuBaseAddress();
    EXPECT_LT(ptrDiff(bufferAddress, baseAddress), MemoryConstants::max32BitAddress);

    EXPECT_TRUE(drmAllocation->is32BitAllocation());

    auto allocationCpuPtr = drmAllocation->getUnderlyingBuffer();
    auto allocationPageOffset = ptrDiff(allocationCpuPtr, alignDown(allocationCpuPtr, MemoryConstants::pageSize));

    auto allocationGpuPtr = drmAllocation->getGpuAddress();
    auto allocationGpuOffset = ptrDiff(allocationGpuPtr, alignDown(allocationGpuPtr, MemoryConstants::pageSize));

    auto bufferObject = drmAllocation->getBO();

    EXPECT_EQ(drmAllocation->getUnderlyingBuffer(), reinterpret_cast<void *>(offsetPtr));

    // Gpu address should be different
    EXPECT_NE(offsetPtr, drmAllocation->getGpuAddress());
    // Gpu address offset iqual to cpu offset
    EXPECT_EQ(allocationGpuOffset, ptrOffset);

    EXPECT_EQ(allocationPageOffset, ptrOffset);

    auto boAddress = bufferObject->peekAddress();
    EXPECT_EQ(alignDown(boAddress, MemoryConstants::pageSize), boAddress);

    delete buffer;
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferCreatedFrom64BitHostPtrThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    {
        if (is32bit) {
            mock->ioctlExpected.total = -1;
        } else {
            mock->ioctlExpected.gemUserptr = 1;
            mock->ioctlExpected.gemWait = 1;
            mock->ioctlExpected.gemClose = 1;

            debugManager.flags.Force32bitAddressing.set(true);
            MockContext context(pClDevice);
            memoryManager->setForce32BitAllocations(true);

            auto size = MemoryConstants::pageSize;
            void *ptr = reinterpret_cast<void *>(0x100000000000);
            auto ptrOffset = MemoryConstants::cacheLineSize;
            uintptr_t offsetPtr = (uintptr_t)ptr + ptrOffset;
            auto retVal = CL_SUCCESS;

            auto buffer = Buffer::create(
                &context,
                CL_MEM_USE_HOST_PTR,
                size,
                reinterpret_cast<void *>(offsetPtr),
                retVal);
            EXPECT_EQ(CL_SUCCESS, retVal);

            EXPECT_TRUE(buffer->isMemObjZeroCopy());
            auto bufferAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();

            auto baseAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuBaseAddress();
            EXPECT_LT(ptrDiff(bufferAddress, baseAddress), MemoryConstants::max32BitAddress);

            auto drmAllocation = static_cast<DrmAllocation *>(buffer->getGraphicsAllocation(rootDeviceIndex));

            EXPECT_TRUE(drmAllocation->is32BitAllocation());

            auto allocationCpuPtr = drmAllocation->getUnderlyingBuffer();
            auto allocationPageOffset = ptrDiff(allocationCpuPtr, alignDown(allocationCpuPtr, MemoryConstants::pageSize));
            auto bufferObject = drmAllocation->getBO();

            EXPECT_EQ(allocationPageOffset, ptrOffset);

            auto boAddress = bufferObject->peekAddress();
            EXPECT_EQ(alignDown(boAddress, MemoryConstants::pageSize), boAddress);

            delete buffer;
            debugManager.flags.Force32bitAddressing.set(false);
        }
    }
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, GivenSizeAbove2GBWhenUseHostPtrAndAllocHostPtrAreCreatedThenFirstSucceedsAndSecondFails) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctlExpected.total = -1;
    debugManager.flags.Force32bitAddressing.set(true);
    MockContext context(pClDevice);
    memoryManager->setForce32BitAllocations(true);

    size_t size = 2 * MemoryConstants::gigaByte;
    void *ptr = reinterpret_cast<void *>(0x100000000000);
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size,
        ptr,
        retVal);

    size_t size2 = 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize; // Keep size aligned

    auto buffer2 = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size2,
        nullptr,
        retVal);

    EXPECT_NE(retVal, CL_SUCCESS);
    EXPECT_EQ(nullptr, buffer2);

    if (buffer) {
        auto bufferPtr = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();

        EXPECT_TRUE(buffer->getGraphicsAllocation(rootDeviceIndex)->is32BitAllocation());
        auto baseAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuBaseAddress();
        EXPECT_LT(ptrDiff(bufferPtr, baseAddress), MemoryConstants::max32BitAddress);
    }

    delete buffer;
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, GivenSizeAbove2GBWhenAllocHostPtrAndUseHostPtrAreCreatedThenFirstSucceedsAndSecondFails) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctlExpected.total = -1;
    debugManager.flags.Force32bitAddressing.set(true);
    MockContext context(pClDevice);
    memoryManager->setForce32BitAllocations(true);

    size_t size = 2 * MemoryConstants::gigaByte;
    void *ptr = reinterpret_cast<void *>(0x100000000000);
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size,
        nullptr,
        retVal);

    size_t size2 = 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize; // Keep size aligned

    auto buffer2 = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size2,
        ptr,
        retVal);

    EXPECT_NE(retVal, CL_SUCCESS);
    EXPECT_EQ(nullptr, buffer2);

    if (buffer) {
        auto bufferPtr = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();

        EXPECT_TRUE(buffer->getGraphicsAllocation(rootDeviceIndex)->is32BitAllocation());
        auto baseAddress = buffer->getGraphicsAllocation(rootDeviceIndex)->getGpuBaseAddress();
        EXPECT_LT(ptrDiff(bufferPtr, baseAddress), MemoryConstants::max32BitAddress);
    }

    delete buffer;
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmBufferWhenItIsQueriedForInternalAllocationThenBoIsReturned) {
    mock->ioctlExpected.total = -1;
    mock->outputFd = 1337;
    MockContext context(pClDevice);

    size_t size = 1u;
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size,
        nullptr,
        retVal);

    uint64_t handle = 0llu;

    retVal = clGetMemObjectInfo(buffer, CL_MEM_ALLOCATION_HANDLE_INTEL, sizeof(handle), &handle, nullptr);
    EXPECT_EQ(retVal, CL_SUCCESS);

    EXPECT_EQ(static_cast<uint64_t>(1337), handle);

    clReleaseMemObject(buffer);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageWithMipCountZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    if (!defaultHwInfo->capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    mock->ioctlExpected.gemCreate = 1;
    mock->ioctlExpected.gemSetTiling = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    MockContext context(pClDevice);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);
    auto imageSize = drmAllocation->getUnderlyingBufferSize();
    auto rowPitch = dstImage->getImageDesc().image_row_pitch;

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imageSize, this->mock->createParamsSize);
    auto ioctlHelper = this->mock->getIoctlHelper();
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::tilingY);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageWithMipCountNonZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    if (!defaultHwInfo->capabilityTable.supportsImages) {
        GTEST_SKIP();
    }
    mock->ioctlExpected.gemCreate = 1;
    mock->ioctlExpected.gemSetTiling = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    MockContext context(pClDevice);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;
    imageDesc.num_mip_levels = 1u;

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<uint32_t>(imageDesc.num_mip_levels), dstImage->peekMipCount());
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);
    auto imageSize = drmAllocation->getUnderlyingBufferSize();
    auto rowPitch = dstImage->getImageDesc().image_row_pitch;

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imageSize, this->mock->createParamsSize);
    auto ioctlHelper = this->mock->getIoctlHelper();
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::tilingY);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageIsBeingCreatedAndAllocationFailsThenReturnNullptr) {
    MockContext context(pClDevice);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto retVal = CL_SUCCESS;

    InjectedFunction method = [&](size_t failureIndex) {
        cl_mem_flags flags = CL_MEM_WRITE_ONLY;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        std::unique_ptr<Image> dstImage(Image::create(
            &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
            flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_NE(nullptr, dstImage.get());
        } else {
            EXPECT_EQ(nullptr, dstImage.get());
        }
    };

    injectFailures(method);
    mock->reset();
}

HWTEST2_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenAllocateGraphicsMemoryForImageIsUsed, IsAtMostXe3Core) {
    if (!defaultHwInfo->capabilityTable.supportsImages) {
        GTEST_SKIP();
    }

    device->setPreemptionMode(PreemptionMode::Disabled);

    mock->ioctlExpected.gemCreate = 1;
    mock->ioctlExpected.gemSetTiling = 1;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 1;

    // builtins kernels
    mock->ioctlExpected.gemUserptr += 7;

    // command buffers
    mock->ioctlExpected.gemUserptr += 2;
    additionalDestroyDeviceIoctls.gemClose += 2;
    additionalDestroyDeviceIoctls.gemWait += 2;

    // indirect heaps
    mock->ioctlExpected.gemUserptr += 3;
    additionalDestroyDeviceIoctls.gemClose += 3;
    additionalDestroyDeviceIoctls.gemWait += 3;

    if (device->getDefaultEngine().commandStreamReceiver->peekTimestampPacketWriteEnabled()) {
        mock->ioctlExpected.gemUserptr++;
        additionalDestroyDeviceIoctls.gemClose++;
        additionalDestroyDeviceIoctls.gemWait++;
    }

    if (device->getDefaultEngine().commandStreamReceiver->getClearColorAllocation() != nullptr) {
        mock->ioctlExpected.gemUserptr++;
        additionalDestroyDeviceIoctls.gemClose++;
        additionalDestroyDeviceIoctls.gemWait++;
    }

    MockContext context(pClDevice);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 64u;
    imageDesc.image_height = 64u;

    auto data = alignedMalloc(64u * 64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(imageGraphicsAllocation);
    auto imageSize = drmAllocation->getUnderlyingBufferSize();
    auto rowPitch = dstImage->getImageDesc().image_row_pitch;

    EXPECT_EQ(1u, this->mock->createParamsHandle);
    EXPECT_EQ(imageSize, this->mock->createParamsSize);
    auto ioctlHelper = this->mock->getIoctlHelper();
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::tilingY);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);

    alignedFree(data);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenMemoryAllocatedForImageThenUnmapSizeCorrectlySetWhenLimitedRangeAllocationUsedOrNotUsed) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    MockContext context(pClDevice);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    auto data = alignedMalloc(64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, imageGraphicsAllocation);

    alignedFree(data);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenNonTiledImgWithMipCountZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    MockContext context(pClDevice);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    auto data = alignedMalloc(64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;
    this->mock->createParamsHandle = 0;
    this->mock->createParamsSize = 0;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    EXPECT_EQ(0u, this->mock->createParamsHandle);
    EXPECT_EQ(0u, this->mock->createParamsSize);
    auto ioctlHelper = this->mock->getIoctlHelper();
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::tilingNone);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenNonTiledImgWithMipCountNonZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    MockContext context(pClDevice);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;
    imageDesc.num_mip_levels = 1u;

    auto data = alignedMalloc(64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;

    this->mock->createParamsHandle = 0;
    this->mock->createParamsSize = 0;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, data, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, dstImage);
    EXPECT_EQ(static_cast<uint32_t>(imageDesc.num_mip_levels), dstImage->peekMipCount());
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, imageGraphicsAllocation);
    EXPECT_TRUE(imageGraphicsAllocation->getDefaultGmm()->resourceParams.Usage ==
                GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);

    EXPECT_EQ(0u, this->mock->createParamsHandle);
    EXPECT_EQ(0u, this->mock->createParamsSize);
    auto ioctlHelper = this->mock->getIoctlHelper();
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::tilingNone);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhen1DarrayImageIsBeingCreatedFromHostPtrThenTilingIsNotCalled) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    MockContext context(pClDevice);

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc = {};

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 64u;

    auto data = alignedMalloc(64u * 4 * 8, MemoryConstants::pageSize);

    auto retVal = CL_SUCCESS;
    this->mock->createParamsHandle = 0;
    this->mock->createParamsSize = 0;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    std::unique_ptr<Image> dstImage(Image::create(
        &context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
        flags, 0, surfaceFormat, &imageDesc, data, retVal));
    auto imageGraphicsAllocation = dstImage->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, imageGraphicsAllocation);

    EXPECT_EQ(0u, this->mock->createParamsHandle);
    EXPECT_EQ(0u, this->mock->createParamsSize);
    auto ioctlHelper = this->mock->getIoctlHelper();
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::tilingNone);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenHostPointerNotRequiringCopyWhenAllocateGraphicsMemoryForImageIsCalledThenGraphicsAllocationIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image1D;
    imgDesc.imageWidth = MemoryConstants::pageSize;
    imgDesc.imageHeight = 1;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    MockContext context(pClDevice);
    auto surfaceFormat = &Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features)->surfaceFormat;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, surfaceFormat);
    imgInfo.rowPitch = imgDesc.imageWidth * surfaceFormat->imageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.imageHeight;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = true;

    auto hostPtr = alignedMalloc(imgDesc.imageWidth * imgDesc.imageHeight * 4, MemoryConstants::pageSize);
    bool copyRequired = MockMemoryManager::isCopyRequired(imgInfo, hostPtr);
    EXPECT_FALSE(copyRequired);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.hostPtr = hostPtr;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto imageAllocation = memoryManager->allocateGraphicsMemoryForImage(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_EQ(hostPtr, imageAllocation->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemory(imageAllocation);
    alignedFree(hostPtr);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenOsHandleWithNonTiledObjectWhenCreateFromSharedHandleIsCalledThenNonTiledGmmIsCreatedAndSetInAllocation) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemGetTiling = 1;
    auto ioctlHelper = this->mock->getIoctlHelper();
    mock->getTilingModeOut = ioctlHelper->getDrmParamValue(DrmParam::tilingNone);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    uint32_t boHandle = 2u;
    mock->outputHandle = boHandle;

    cl_mem_flags flags = CL_MEM_READ_ONLY;
    cl_image_desc imgDesc = {};
    cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    const ClSurfaceFormatInfo *gmmSurfaceFormat = nullptr;
    ImageInfo imgInfo = {};

    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    imgInfo.imgDesc = Image::convertDescriptor(imgDesc);
    MockContext context(pClDevice);
    gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &gmmImgFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    imgInfo.surfaceFormat = &gmmSurfaceFormat->surfaceFormat;
    imgInfo.plane = GMM_PLANE_Y;

    AllocationProperties properties(rootDeviceIndex, false, &imgInfo, AllocationType::sharedImage, context.getDevice(0)->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(boHandle, mock->getTilingHandleIn);
    EXPECT_EQ(AllocationType::sharedImage, graphicsAllocation->getAllocationType());

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.TiledY);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenOsHandleWithTileYObjectWhenCreateFromSharedHandleIsCalledThenTileYGmmIsCreatedAndSetInAllocation) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemGetTiling = 1;
    auto ioctlHelper = this->mock->getIoctlHelper();
    mock->getTilingModeOut = ioctlHelper->getDrmParamValue(DrmParam::tilingY);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    uint32_t boHandle = 2u;
    mock->outputHandle = boHandle;

    cl_mem_flags flags = CL_MEM_READ_ONLY;
    cl_image_desc imgDesc = {};
    cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    const ClSurfaceFormatInfo *gmmSurfaceFormat = nullptr;
    ImageInfo imgInfo = {};

    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    imgInfo.imgDesc = Image::convertDescriptor(imgDesc);
    MockContext context(pClDevice);
    gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &gmmImgFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    imgInfo.surfaceFormat = &gmmSurfaceFormat->surfaceFormat;
    imgInfo.plane = GMM_PLANE_Y;

    AllocationProperties properties(rootDeviceIndex, false, &imgInfo, AllocationType::sharedImage, context.getDevice(0)->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(boHandle, mock->getTilingHandleIn);
    EXPECT_EQ(AllocationType::sharedImage, graphicsAllocation->getAllocationType());

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.Linear);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenCreateFromSharedHandleFailsToCallGetTilingThenNonLinearStorageIsAssumed) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemGetTiling = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    this->ioctlResExt = {mock->ioctlCnt.total + 1, -1};
    mock->ioctlResExt = &ioctlResExt;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    uint32_t boHandle = 2u;
    mock->outputHandle = boHandle;

    cl_mem_flags flags = CL_MEM_READ_ONLY;
    cl_image_desc imgDesc = {};
    cl_image_format gmmImgFormat = {CL_NV12_INTEL, CL_UNORM_INT8};
    const ClSurfaceFormatInfo *gmmSurfaceFormat = nullptr;
    ImageInfo imgInfo = {};

    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    imgInfo.imgDesc = Image::convertDescriptor(imgDesc);
    MockContext context(pClDevice);
    gmmSurfaceFormat = Image::getSurfaceFormatFromTable(flags, &gmmImgFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    imgInfo.surfaceFormat = &gmmSurfaceFormat->surfaceFormat;
    imgInfo.plane = GMM_PLANE_Y;

    AllocationProperties properties(rootDeviceIndex, false, &imgInfo, AllocationType::sharedImage, context.getDevice(0)->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(boHandle, mock->getTilingHandleIn);
    EXPECT_EQ(AllocationType::sharedImage, graphicsAllocation->getAllocationType());

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.Linear);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST(DrmMemoryMangerTest, givenMultipleRootDeviceWhenMemoryManagerGetsDrmThenDrmIsFromCorrectRootDevice) {
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(4);
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    initPlatform();

    TestedDrmMemoryManager drmMemoryManager(*platform()->peekExecutionEnvironment());
    for (auto i = 0u; i < platform()->peekExecutionEnvironment()->rootDeviceEnvironments.size(); i++) {
        auto drmFromRootDevice = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Drm>();
        EXPECT_EQ(drmFromRootDevice, &drmMemoryManager.getDrm(i));
        EXPECT_EQ(i, drmMemoryManager.getRootDeviceIndex(drmFromRootDevice));
    }
    EXPECT_EQ(CommonConstants::unspecifiedDeviceIndex, drmMemoryManager.getRootDeviceIndex(nullptr));
}
