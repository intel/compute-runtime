/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/api/api.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_image.h"

namespace NEO {
extern ImageFactoryFuncs imageFactory[IGFX_MAX_CORE];
}

using namespace NEO;

using PvcAndLaterImageTests = ::testing::Test;

template <typename T>
struct MockImage : public ImageHw<T> {
    using ImageHw<T>::transferData;
    using ImageHw<T>::ImageHw;

    static Image *createMockImage(Context *context,
                                  const MemoryProperties &memoryProperties,
                                  uint64_t flags,
                                  uint64_t flagsIntel,
                                  size_t size,
                                  void *hostPtr,
                                  const cl_image_format &imageFormat,
                                  const cl_image_desc &imageDesc,
                                  bool zeroCopy,
                                  MultiGraphicsAllocation multiGraphicsAllocation,
                                  bool isObjectRedescribed,
                                  uint32_t baseMipLevel,
                                  uint32_t mipCount,
                                  const ClSurfaceFormatInfo *surfaceFormatInfo,
                                  const SurfaceOffsets *surfaceOffsets) {
        auto memoryStorage = multiGraphicsAllocation.getDefaultGraphicsAllocation()->getUnderlyingBuffer();
        return new MockImage<T>(context, memoryProperties, flags, flagsIntel, size, memoryStorage, hostPtr, imageFormat, imageDesc,
                                zeroCopy, std::move(multiGraphicsAllocation), isObjectRedescribed, baseMipLevel, mipCount,
                                *surfaceFormatInfo, surfaceOffsets);
    }

    void transferData(void *dst, size_t dstRowPitch, size_t dstSlicePitch,
                      void *src, size_t srcRowPitch, size_t srcSlicePitch,
                      std::array<size_t, 3> copyRegion, std::array<size_t, 3> copyOrigin) override {
        transferDataDestinationPointers.push_back(dst);
        ImageHw<T>::transferData(dst, dstRowPitch, dstSlicePitch, src, srcRowPitch, srcSlicePitch, copyRegion, copyOrigin);
    }

    std::vector<void *> transferDataDestinationPointers;
};

HWTEST2_F(PvcAndLaterImageTests, givenNoImagesSupportLocalMemoryEnabledAndCopyHostPtrWhenCreatingLinearImageThenMemoryIsTransferredOverCpu, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(true);
    auto eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;
    VariableBackup<bool> supportsImagesBackup{&defaultHwInfo->capabilityTable.supportsImages, false};
    VariableBackup<ImageCreateFunc> createImageFunctionBackup{&imageFactory[eRenderCoreFamily].createImageFunction};
    createImageFunctionBackup = MockImage<FamilyType>::createMockImage;

    uint32_t devicesCount = 1;
    UltClDeviceFactory clDeviceFactory{devicesCount, 0};
    cl_device_id devices[] = {clDeviceFactory.rootDevices[0]};
    MockContext context{ClDeviceVector{devices, 1}};
    uint8_t imageMemory[5] = {1, 2, 3, 4, 5};
    cl_int retVal = CL_INVALID_VALUE;
    cl_image_format format = {0};
    format.image_channel_data_type = CL_UNSIGNED_INT8;
    format.image_channel_order = CL_R;
    cl_image_desc desc{0};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = 5;
    desc.image_height = 1;
    desc.image_depth = 1;
    cl_mem image = clCreateImageWithPropertiesINTEL(&context, nullptr, CL_MEM_COPY_HOST_PTR | CL_MEM_FORCE_LINEAR_STORAGE_INTEL, &format, &desc, imageMemory, &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, image);

    auto &mockImage = *static_cast<MockImage<FamilyType> *>(image);
    auto &graphicsAllocations = mockImage.getMultiGraphicsAllocation().getGraphicsAllocations();
    EXPECT_FALSE(graphicsAllocations[0]->isLocked());

    auto &mockMemoryManager = static_cast<MockMemoryManager &>(*clDeviceFactory.rootDevices[0]->getMemoryManager());
    EXPECT_EQ(graphicsAllocations.size(), mockMemoryManager.lockResourceCalled);
    EXPECT_EQ(graphicsAllocations.size(), mockMemoryManager.unlockResourceCalled);

    EXPECT_EQ(devicesCount, mockMemoryManager.lockResourcePointers.size());
    EXPECT_EQ(devicesCount, mockImage.transferDataDestinationPointers.size());
    EXPECT_EQ(mockMemoryManager.lockResourcePointers[0], mockImage.transferDataDestinationPointers[0]);

    clReleaseMemObject(image);
}
