/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm.h"
#include "runtime/mem_obj/image.h"
#include "runtime/sharings/unified/unified_image.h"
#include "unit_tests/helpers/raii_hw_helper.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"
#include "unit_tests/sharings/unified/unified_sharing_fixtures.h"
#include "unit_tests/sharings/unified/unified_sharing_mocks.h"

using UnifiedSharingImageTestsWithMemoryManager = UnifiedSharingFixture<true, true>;
using UnifiedSharingImageTestsWithInvalidMemoryManager = UnifiedSharingFixture<true, false>;

static UnifiedSharingMemoryDescription getValidUnifiedSharingDesc() {
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::Win32Nt;
    return desc;
}

static cl_image_format getValidImageFormat() {
    cl_image_format format{};
    format.image_channel_data_type = CL_UNORM_INT8;
    format.image_channel_order = CL_RGBA;
    return format;
}

static cl_image_desc getValidImageDesc() {
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 128;
    imageDesc.image_height = 128;
    imageDesc.image_depth = 1;
    imageDesc.image_array_size = 1;
    imageDesc.image_row_pitch = 256;
    imageDesc.image_slice_pitch = 0u;
    imageDesc.num_mip_levels = 1;
    imageDesc.num_samples = 0;
    imageDesc.buffer = nullptr;
    return imageDesc;
}

TEST_F(UnifiedSharingImageTestsWithInvalidMemoryManager, givenValidContextAndAllocationFailsWhenCreatingImageFromSharedHandleThenReturnInvalidMemObject) {
    cl_mem_flags flags{};
    cl_int retVal{};
    const auto format = getValidImageFormat();
    const auto imageDesc = getValidImageDesc();
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                               &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(UnifiedSharingImageTestsWithMemoryManager, givenUnsupportedHandleTypeWhenCreatingImageFromSharedHandleThenReturnInvalidMemObject) {
    cl_mem_flags flags{};
    cl_int retVal{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    const auto format = getValidImageFormat();
    const auto imageDesc = getValidImageDesc();

    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, desc,
                                                                               &format, &imageDesc, &retVal));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    desc.type = UnifiedSharingHandleType::Win32Shared;
    image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, desc,
                                                                          &format, &imageDesc, &retVal));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);

    desc.type = UnifiedSharingHandleType::LinuxFd;
    image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, desc,
                                                                          &format, &imageDesc, &retVal));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(UnifiedSharingImageTestsWithMemoryManager, givenValidContextAndMemoryManagerWhenCreatingImageFromSharedHandleThenReturnSuccess) {
    cl_mem_flags flags{};
    cl_int retVal{};
    const auto format = getValidImageFormat();
    const auto imageDesc = getValidImageDesc();
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                               &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
}

TEST_F(UnifiedSharingImageTestsWithMemoryManager, givenPassedFormatWhenCreatingUnifiedImageThenFormatIsCorrectlySetInImageObject) {
    cl_image_format format{};
    format.image_channel_data_type = CL_HALF_FLOAT;
    format.image_channel_order = CL_RG;

    cl_mem_flags flags{};
    cl_int retVal{};
    const auto imageDesc = getValidImageDesc();
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                               &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(GMM_FORMAT_R16G16_FLOAT_TYPE, image->getSurfaceFormatInfo().surfaceFormat.GMMSurfaceFormat);
    EXPECT_EQ(GFX3DSTATE_SURFACEFORMAT_R16G16_FLOAT, image->getSurfaceFormatInfo().surfaceFormat.GenxSurfaceFormat);
}

template <typename GfxFamily, bool pageTableManagerSupported>
class MockHwHelper : public HwHelperHw<GfxFamily> {
  public:
    bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const override {
        return pageTableManagerSupported;
    }
};

struct MemoryManagerReturningCompressedAllocations : UnifiedSharingMockMemoryManager<true> {
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex) override {
        auto allocation = UnifiedSharingMockMemoryManager<true>::createGraphicsAllocationFromNTHandle(handle, rootDeviceIndex);

        auto gmm = allocation->getDefaultGmm();
        auto mockGmmResourceInfo = std::make_unique<MockGmmResourceInfo>(gmm->gmmResourceInfo->peekHandle());
        mockGmmResourceInfo->setUnifiedAuxTranslationCapable();
        gmm->gmmResourceInfo = std::move(mockGmmResourceInfo);

        return allocation;
    }

    bool mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) override {
        calledMapAuxGpuVA++;
        return resultOfMapAuxGpuVA;
    }

    unsigned int calledMapAuxGpuVA{};
    bool resultOfMapAuxGpuVA{};
};

HWTEST_F(UnifiedSharingImageTestsWithMemoryManager, givenCompressedImageAndNoPageTableManagerWhenCreatingUnifiedImageThenSetCorrespondingFieldInGmmAndDoNotUsePageTableManager) {
    MemoryManagerReturningCompressedAllocations memoryManager{};
    VariableBackup<MemoryManager *> memoryManagerBackup{&this->context->memoryManager, &memoryManager};
    using HwHelperNotSupportingPageTableManager = MockHwHelper<FamilyType, false>;
    RAIIHwHelperFactory<HwHelperNotSupportingPageTableManager> hwHelperBackup{this->context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily};

    cl_mem_flags flags{};
    cl_int retVal{};
    const auto format = getValidImageFormat();
    const auto imageDesc = getValidImageDesc();
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                               &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(image->getGraphicsAllocation()->getDefaultGmm()->isRenderCompressed);
    EXPECT_EQ(0u, memoryManager.calledMapAuxGpuVA);
}

HWTEST_F(UnifiedSharingImageTestsWithMemoryManager, givenCompressedImageAndPageTableManagerWhenCreatingUnifiedImageThenSetCorrespondingFieldInGmmBasedOnAuxGpuVaMappingResult) {
    MemoryManagerReturningCompressedAllocations memoryManager{};
    VariableBackup<MemoryManager *> memoryManagerBackup{&this->context->memoryManager, &memoryManager};
    using HwHelperSupportingPageTableManager = MockHwHelper<FamilyType, true>;
    RAIIHwHelperFactory<HwHelperSupportingPageTableManager> hwHelperBackup{this->context->getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily};

    cl_mem_flags flags{};
    cl_int retVal{};
    const auto format = getValidImageFormat();
    const auto imageDesc = getValidImageDesc();

    memoryManager.resultOfMapAuxGpuVA = true;
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                               &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(memoryManager.resultOfMapAuxGpuVA, image->getGraphicsAllocation()->getDefaultGmm()->isRenderCompressed);
    EXPECT_EQ(1u, memoryManager.calledMapAuxGpuVA);

    memoryManager.resultOfMapAuxGpuVA = false;
    image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                          &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(memoryManager.resultOfMapAuxGpuVA, image->getGraphicsAllocation()->getDefaultGmm()->isRenderCompressed);
    EXPECT_EQ(2u, memoryManager.calledMapAuxGpuVA);
}
