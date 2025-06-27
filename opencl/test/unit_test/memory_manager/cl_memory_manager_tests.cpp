/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/program_initialization.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/fixtures/memory_allocator_fixture.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

using namespace NEO;

using ClMemoryAllocatorTest = ::Test<MemoryAllocatorFixture>;

TEST_F(ClMemoryAllocatorTest, givenStatelessKernelWithPrintfWhenPrintfSurfaceIsCreatedThenPrintfSurfaceIsPatchedWithBaseAddressOffset) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockKernelWithInternals kernel(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel.mockKernel);

    kernel.kernelInfo.setBufferAddressingMode(KernelDescriptor::Stateless);
    kernel.kernelInfo.setPrintfSurface(sizeof(uintptr_t), 8);

    auto printfHandler = PrintfHandler::create(multiDispatchInfo, device->getDevice());

    printfHandler->prepareDispatch(multiDispatchInfo);

    auto printfAllocation = printfHandler->getSurface();
    auto allocationAddress = printfAllocation->getGpuAddressToPatch();

    auto printfPatchAddress = ptrOffset(reinterpret_cast<uintptr_t *>(kernel.mockKernel->getCrossThreadData()),
                                        kernel.mockKernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.stateless);

    EXPECT_EQ(allocationAddress, *(uintptr_t *)printfPatchAddress);

    delete printfHandler;
}

HWTEST_F(ClMemoryAllocatorTest, givenStatefulKernelWithPrintfWhenPrintfSurfaceIsCreatedThenPrintfSurfaceIsPatchedWithCpuAddress) {
    auto rootDeviceIndex = 1u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), rootDeviceIndex));
    MockKernelWithInternals kernel(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel.mockKernel);

    kernel.kernelInfo.setPrintfSurface(sizeof(uintptr_t), 8, 16);

    auto printfHandler = PrintfHandler::create(multiDispatchInfo, device->getDevice());

    printfHandler->prepareDispatch(multiDispatchInfo);

    auto printfAllocation = printfHandler->getSurface();
    auto allocationAddress = printfAllocation->getGpuAddress();

    EXPECT_NE(0u, kernel.mockKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(kernel.mockKernel->getSurfaceStateHeap(),
                  kernel.mockKernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindful));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(allocationAddress, surfaceAddress);

    delete printfHandler;
}

TEST_F(ClMemoryAllocatorTest, given32BitDeviceWhenPrintfSurfaceIsCreatedThen32BitAllocationsIsMade) {
    DebugManagerStateRestore dbgRestorer;
    if constexpr (is64bit) {
        debugManager.flags.Force32bitAddressing.set(true);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

        MockKernelWithInternals kernel(*device);
        kernel.kernelInfo.setPrintfSurface(4, 0);
        for (int i = 0; i < 8; i++) {
            kernel.mockKernel->mockCrossThreadData[i] = 50;
        }

        MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel.mockKernel);

        auto printfHandler = PrintfHandler::create(multiDispatchInfo, device->getDevice());
        printfHandler->prepareDispatch(multiDispatchInfo);

        uint32_t *ptr32Bit = (uint32_t *)kernel.mockKernel->mockCrossThreadData.data();
        auto printfAllocation = printfHandler->getSurface();
        auto allocationAddress = printfAllocation->getGpuAddressToPatch();
        uint32_t allocationAddress32bit = (uint32_t)(uintptr_t)allocationAddress;

        EXPECT_TRUE(printfAllocation->is32BitAllocation());
        EXPECT_EQ(allocationAddress32bit, *ptr32Bit);
        for (int i = 4; i < 8; i++) {
            EXPECT_EQ(50, kernel.mockKernel->mockCrossThreadData[i]);
        }

        delete printfHandler;

        debugManager.flags.Force32bitAddressing.set(false);
    }
}

TEST(ClOsAgnosticMemoryManager, givenHostPointerNotRequiringCopyWhenAllocateGraphicsMemoryForImageFromHostPtrIsCalledThenGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 4;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::image1D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features)->surfaceFormat;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat);
    imgInfo.rowPitch = imgDesc.imageWidth * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.imageHeight;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = true;

    auto hostPtr = alignedMalloc(imgDesc.imageWidth * imgDesc.imageHeight * 4, MemoryConstants::pageSize);

    bool copyRequired = MockMemoryManager::isCopyRequired(imgInfo, hostPtr);
    EXPECT_FALSE(copyRequired);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.hostPtr = hostPtr;
    allocationData.size = imgInfo.size;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImageFromHostPtr(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_EQ(hostPtr, imageAllocation->getUnderlyingBuffer());

    memoryManager.freeGraphicsMemory(imageAllocation);
    alignedFree(hostPtr);
}

TEST(ClOsAgnosticMemoryManager, givenHostPointerRequiringCopyWhenAllocateGraphicsMemoryForImageFromHostPtrIsCalledThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment{};
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    ImageDescriptor imgDesc = {};
    imgDesc.imageHeight = 4;
    imgDesc.imageWidth = 4;
    imgDesc.imageType = ImageType::image2D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat->surfaceFormat);
    imgInfo.rowPitch = imgDesc.imageWidth * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.imageHeight;
    imgInfo.size = imgInfo.slicePitch;

    auto hostPtr = alignedMalloc(imgDesc.imageWidth * imgDesc.imageHeight * 4, MemoryConstants::pageSize);

    bool copyRequired = MockMemoryManager::isCopyRequired(imgInfo, hostPtr);
    EXPECT_TRUE(copyRequired);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.hostPtr = hostPtr;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImageFromHostPtr(allocationData);
    EXPECT_EQ(nullptr, imageAllocation);

    alignedFree(hostPtr);
}

TEST(ClMemoryManagerTest, givenAllowedTilingWhenIsCopyRequiredIsCalledThenTrueIsReturned) {
    ImageInfo imgInfo{};
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 1;
    imageDesc.image_height = 1;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;

    char memory{};

    EXPECT_TRUE(MockMemoryManager::isCopyRequired(imgInfo, &memory));
}

TEST(ClMemoryManagerTest, givenDifferentRowPitchWhenIsCopyRequiredIsCalledThenTrueIsReturned) {
    ImageInfo imgInfo{};
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1;
    imageDesc.image_height = 1;
    imageDesc.image_row_pitch = 10;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;

    char memory[10] = {};

    EXPECT_TRUE(MockMemoryManager::isCopyRequired(imgInfo, memory));
}

TEST(ClMemoryManagerTest, givenDifferentSlicePitchAndTilingNotAllowedWhenIsCopyRequiredIsCalledThenTrueIsReturned) {
    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 4;
    imageDesc.image_height = 2;
    imageDesc.image_slice_pitch = imageDesc.image_width * (imageDesc.image_height + 3) * surfaceFormat->surfaceFormat.imageElementSizeInBytes;

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    char memory[8] = {};

    EXPECT_TRUE(MockMemoryManager::isCopyRequired(imgInfo, memory));
}

TEST(ClMemoryManagerTest, givenNotCachelinAlignedPointerWhenIsCopyRequiredIsCalledThenTrueIsReturned) {
    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 4096;
    imageDesc.image_height = 1;

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    char memory[8] = {};

    EXPECT_TRUE(MockMemoryManager::isCopyRequired(imgInfo, &memory[1]));
}

TEST(ClMemoryManagerTest, givenCachelineAlignedPointerAndProperDescriptorValuesWhenIsCopyRequiredIsCalledThenFalseIsReturned) {
    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 2;
    imageDesc.image_height = 1;

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = true;

    auto hostPtr = alignedMalloc(imgInfo.size, MemoryConstants::cacheLineSize);

    EXPECT_FALSE(MockMemoryManager::isCopyRequired(imgInfo, hostPtr));
    alignedFree(hostPtr);
}

TEST(ClMemoryManagerTest, givenForcedLinearImages3DImageAndProperDescriptorValuesWhenIsCopyRequiredIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ForceLinearImages.set(true);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 2;
    imageDesc.image_height = 2;
    imageDesc.image_depth = 2;

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.imageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = productHelper.isLinearStoragePreferred(Image::isImage1d(Image::convertDescriptor(imgInfo.imgDesc)), false);

    auto hostPtr = alignedMalloc(imgInfo.size, MemoryConstants::cacheLineSize);

    EXPECT_FALSE(MockMemoryManager::isCopyRequired(imgInfo, hostPtr));
    alignedFree(hostPtr);
}

TEST(ClOsAgnosticMemoryManager, givenUseExternalAllocatorForSshAndDshWhenMemoryManagerCreatedThenExternalHeapIsNotallowed) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);

    MockExecutionEnvironment executionEnvironment{};
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.heapAssigners[0]->apiAllowExternalHeapForSshAndDsh);
}

using ClMemoryManagerMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(ClMemoryManagerMultiRootDeviceTests, WhenAllocatingGlobalSurfaceThenItHasCorrectRootDeviceIndex) {
    if (device1->getMemoryManager()->isLimitedRange(expectedRootDeviceIndex)) {
        delete context->svmAllocsManager;
        context->svmAllocsManager = nullptr;
    }

    std::vector<unsigned char> initData(1024, 0x5B);
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.exportsGlobalConstants = true;
    linkerInput.traits.exportsGlobalVariables = true;
    GraphicsAllocation *allocation = allocateGlobalsSurface(context->svmAllocsManager, device1->getDevice(), initData.size(), 0u, false, &linkerInput, initData.data());

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectedRootDeviceIndex, allocation->getRootDeviceIndex());

    if (device1->getMemoryManager()->isLimitedRange(expectedRootDeviceIndex)) {
        device1->getMemoryManager()->freeGraphicsMemory(allocation);
    } else {
        context->getSVMAllocsManager()->freeSVMAlloc(reinterpret_cast<void *>(allocation->getGpuAddress()));
    }
}

TEST_F(ClMemoryManagerMultiRootDeviceTests, GivenMultipleRootDevicesWhenMemoryManagerInitializedThenIsaLocalMemoryPlacementIsCheckedForEveryRootDevice) {
    EXPECT_EQ(3u, mockMemoryManager->isaInLocalMemory.size());
}
