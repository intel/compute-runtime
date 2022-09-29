/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_command_stream.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
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
    void SetUp() override {
        MemoryManagementFixture::setUp();

        executionEnvironment = MockClDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        DrmMemoryManagerFixture::setUp(new DrmMockCustom(*executionEnvironment->rootDeviceEnvironments[0]), false);
        pClDevice = new MockClDevice{device}; // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
        device->incRefInternal();
    }
    void TearDown() override {
        delete pClDevice;
        DrmMemoryManagerTest::TearDown();
    }

    MockClDevice *pClDevice = nullptr;
};

TEST_F(ClDrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferAllocationThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    DebugManager.flags.Force32bitAddressing.set(true);
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

TEST_F(ClDrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferCreatedFromHostPtrThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    DebugManager.flags.Force32bitAddressing.set(true);
    MockContext context(pClDevice);
    memoryManager->setForce32BitAllocations(true);

    auto size = MemoryConstants::pageSize;
    void *ptr = reinterpret_cast<void *>(0x1000);
    auto ptrOffset = MemoryConstants::cacheLineSize;
    uintptr_t offsetedPtr = (uintptr_t)ptr + ptrOffset;
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size,
        reinterpret_cast<void *>(offsetedPtr),
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

    EXPECT_EQ(drmAllocation->getUnderlyingBuffer(), reinterpret_cast<void *>(offsetedPtr));

    // Gpu address should be different
    EXPECT_NE(offsetedPtr, drmAllocation->getGpuAddress());
    // Gpu address offset iqual to cpu offset
    EXPECT_EQ(allocationGpuOffset, ptrOffset);

    EXPECT_EQ(allocationPageOffset, ptrOffset);

    auto boAddress = bufferObject->peekAddress();
    EXPECT_EQ(alignDown(boAddress, MemoryConstants::pageSize), boAddress);

    delete buffer;
}

TEST_F(ClDrmMemoryManagerTest, Given32bitAllocatorWhenAskedForBufferCreatedFrom64BitHostPtrThen32BitBufferIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    {
        if (is32bit) {
            mock->ioctl_expected.total = -1;
        } else {
            mock->ioctl_expected.gemUserptr = 1;
            mock->ioctl_expected.gemWait = 1;
            mock->ioctl_expected.gemClose = 1;

            DebugManager.flags.Force32bitAddressing.set(true);
            MockContext context(pClDevice);
            memoryManager->setForce32BitAllocations(true);

            auto size = MemoryConstants::pageSize;
            void *ptr = reinterpret_cast<void *>(0x100000000000);
            auto ptrOffset = MemoryConstants::cacheLineSize;
            uintptr_t offsetedPtr = (uintptr_t)ptr + ptrOffset;
            auto retVal = CL_SUCCESS;

            auto buffer = Buffer::create(
                &context,
                CL_MEM_USE_HOST_PTR,
                size,
                reinterpret_cast<void *>(offsetedPtr),
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
            DebugManager.flags.Force32bitAddressing.set(false);
        }
    }
}

TEST_F(ClDrmMemoryManagerTest, GivenSizeAbove2GBWhenUseHostPtrAndAllocHostPtrAreCreatedThenFirstSucceedsAndSecondFails) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctl_expected.total = -1;
    DebugManager.flags.Force32bitAddressing.set(true);
    MockContext context(pClDevice);
    memoryManager->setForce32BitAllocations(true);

    size_t size = 2 * GB;
    void *ptr = reinterpret_cast<void *>(0x100000000000);
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_USE_HOST_PTR,
        size,
        ptr,
        retVal);

    size_t size2 = 4 * GB - MemoryConstants::pageSize; // Keep size aligned

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

TEST_F(ClDrmMemoryManagerTest, GivenSizeAbove2GBWhenAllocHostPtrAndUseHostPtrAreCreatedThenFirstSucceedsAndSecondFails) {
    DebugManagerStateRestore dbgRestorer;
    mock->ioctl_expected.total = -1;
    DebugManager.flags.Force32bitAddressing.set(true);
    MockContext context(pClDevice);
    memoryManager->setForce32BitAllocations(true);

    size_t size = 2 * GB;
    void *ptr = reinterpret_cast<void *>(0x100000000000);
    auto retVal = CL_SUCCESS;

    auto buffer = Buffer::create(
        &context,
        CL_MEM_ALLOC_HOST_PTR,
        size,
        nullptr,
        retVal);

    size_t size2 = 4 * GB - MemoryConstants::pageSize; // Keep size aligned

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

TEST_F(ClDrmMemoryManagerTest, givenDrmBufferWhenItIsQueriedForInternalAllocationThenBoIsReturned) {
    mock->ioctl_expected.total = -1;
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

HWTEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageWithMipCountZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    if (!UnitTestHelper<FamilyType>::tiledImagesSupported) {
        GTEST_SKIP();
    }
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

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
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::TilingY);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);
}

HWTEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageWithMipCountNonZeroIsBeingCreatedThenallocateGraphicsMemoryForImageIsUsed) {
    if (!UnitTestHelper<FamilyType>::tiledImagesSupported) {
        GTEST_SKIP();
    }
    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

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
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::TilingY);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);
}

TEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageIsBeingCreatedAndAllocationFailsThenReturnNullptr) {
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

HWTEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenTiledImageIsBeingCreatedFromHostPtrThenAllocateGraphicsMemoryForImageIsUsed) {
    if (!UnitTestHelper<FamilyType>::tiledImagesSupported) {
        GTEST_SKIP();
    }

    device->setPreemptionMode(PreemptionMode::Disabled);

    auto csr = static_cast<TestedDrmCommandStreamReceiver<DEFAULT_TEST_FAMILY_NAME> *>(device->getDefaultEngine().commandStreamReceiver);
    csr->flushInternalCallBase = false;

    mock->ioctl_expected.gemCreate = 1;
    mock->ioctl_expected.gemSetTiling = 1;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.execbuffer2 = 0;

    // builtins kernels
    mock->ioctl_expected.gemUserptr += 5;

    // command buffers
    mock->ioctl_expected.gemUserptr += 2;
    additionalDestroyDeviceIoctls.gemClose += 2;
    additionalDestroyDeviceIoctls.gemWait += 2;

    // indirect heaps
    mock->ioctl_expected.gemUserptr += 3;
    additionalDestroyDeviceIoctls.gemClose += 3;
    additionalDestroyDeviceIoctls.gemWait += 3;

    if (device->getDefaultEngine().commandStreamReceiver->peekTimestampPacketWriteEnabled()) {
        mock->ioctl_expected.gemUserptr++;
        additionalDestroyDeviceIoctls.gemClose++;
        additionalDestroyDeviceIoctls.gemWait++;
    }

    if (device->getDefaultEngine().commandStreamReceiver->getClearColorAllocation() != nullptr) {
        mock->ioctl_expected.gemUserptr++;
        additionalDestroyDeviceIoctls.gemClose++;
        additionalDestroyDeviceIoctls.gemWait++;
    }

    MockContext context(pClDevice);

    auto testedCsr = static_cast<TestedDrmCommandStreamReceiver<FamilyType> *>(context.getSpecialQueue(rootDeviceIndex)->getGpgpuEngine().commandStreamReceiver);
    testedCsr->logicalStateHelper.reset(LogicalStateHelper::create<FamilyType>());

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
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::TilingY);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(rowPitch, this->mock->setTilingStride);
    EXPECT_EQ(1u, this->mock->setTilingHandle);

    alignedFree(data);
}

TEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenMemoryAllocatedForImageThenUnmapSizeCorrectlySetWhenLimitedRangeAllocationUsedOrNotUsed) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;

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

TEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenNonTiledImgWithMipCountZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;

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
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::TilingNone);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
}

TEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenNonTiledImgWithMipCountNonZeroisBeingCreatedThenAllocateGraphicsMemoryIsUsed) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

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
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::TilingNone);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
}

TEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhen1DarrayImageIsBeingCreatedFromHostPtrThenTilingIsNotCalled) {
    mock->ioctl_expected.gemUserptr = 2;
    mock->ioctl_expected.gemWait = 2;
    mock->ioctl_expected.gemClose = 2;

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
    uint32_t tilingMode = ioctlHelper->getDrmParamValue(DrmParam::TilingNone);
    EXPECT_EQ(tilingMode, this->mock->setTilingMode);
    EXPECT_EQ(0u, this->mock->setTilingStride);
    EXPECT_EQ(0u, this->mock->setTilingHandle);

    EXPECT_EQ(Sharing::nonSharedResource, imageGraphicsAllocation->peekSharedHandle());

    alignedFree(data);
}

TEST_F(ClDrmMemoryManagerTest, givenHostPointerNotRequiringCopyWhenAllocateGraphicsMemoryForImageIsCalledThenGraphicsAllocationIsReturned) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::Image1D;
    imgDesc.imageWidth = MemoryConstants::pageSize;
    imgDesc.imageHeight = 1;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    MockContext context(pClDevice);
    auto surfaceFormat = &Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features)->surfaceFormat;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, surfaceFormat);
    imgInfo.rowPitch = imgDesc.imageWidth * surfaceFormat->ImageElementSizeInBytes;
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

TEST_F(ClDrmMemoryManagerTest, givenOsHandleWithNonTiledObjectWhenCreateFromSharedHandleIsCalledThenNonTiledGmmIsCreatedAndSetInAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemGetTiling = 1;
    auto ioctlHelper = this->mock->getIoctlHelper();
    mock->getTilingModeOut = ioctlHelper->getDrmParamValue(DrmParam::TilingNone);

    osHandle handle = 1u;
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

    AllocationProperties properties(rootDeviceIndex, false, imgInfo, AllocationType::SHARED_IMAGE, context.getDevice(0)->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(boHandle, mock->getTilingHandleIn);
    EXPECT_EQ(AllocationType::SHARED_IMAGE, graphicsAllocation->getAllocationType());

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    EXPECT_EQ(1u, gmm->resourceParams.Flags.Info.Linear);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.TiledY);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(ClDrmMemoryManagerTest, givenOsHandleWithTileYObjectWhenCreateFromSharedHandleIsCalledThenTileYGmmIsCreatedAndSetInAllocation) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    mock->ioctl_expected.gemGetTiling = 1;
    auto ioctlHelper = this->mock->getIoctlHelper();
    mock->getTilingModeOut = ioctlHelper->getDrmParamValue(DrmParam::TilingY);

    osHandle handle = 1u;
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

    AllocationProperties properties(rootDeviceIndex, false, imgInfo, AllocationType::SHARED_IMAGE, context.getDevice(0)->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(boHandle, mock->getTilingHandleIn);
    EXPECT_EQ(AllocationType::SHARED_IMAGE, graphicsAllocation->getAllocationType());

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.Linear);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(ClDrmMemoryManagerTest, givenDrmMemoryManagerWhenCreateFromSharedHandleFailsToCallGetTilingThenNonLinearStorageIsAssumed) {
    mock->ioctl_expected.primeFdToHandle = 1;
    mock->ioctl_expected.gemGetTiling = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;
    this->ioctlResExt = {mock->ioctl_cnt.total + 1, -1};
    mock->ioctl_res_ext = &ioctlResExt;

    osHandle handle = 1u;
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

    AllocationProperties properties(rootDeviceIndex, false, imgInfo, AllocationType::SHARED_IMAGE, context.getDevice(0)->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, false);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(boHandle, mock->getTilingHandleIn);
    EXPECT_EQ(AllocationType::SHARED_IMAGE, graphicsAllocation->getAllocationType());

    auto gmm = graphicsAllocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.Linear);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST(DrmMemoryMangerTest, givenMultipleRootDeviceWhenMemoryManagerGetsDrmThenDrmIsFromCorrectRootDevice) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(4);
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
