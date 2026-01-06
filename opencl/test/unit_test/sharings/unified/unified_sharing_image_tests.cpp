/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sharings/unified/unified_image.h"
#include "opencl/test/unit_test/sharings/unified/unified_sharing_fixtures.h"

namespace NEO {
class MemoryManager;
} // namespace NEO

using namespace NEO;

using UnifiedSharingImageTestsWithMemoryManager = UnifiedSharingFixture<true, true>;
using UnifiedSharingImageTestsWithInvalidMemoryManager = UnifiedSharingFixture<true, false>;

static UnifiedSharingMemoryDescription getValidUnifiedSharingDesc() {
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::win32Nt;
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
}

TEST_F(UnifiedSharingImageTestsWithMemoryManager, givenValidContextAndMemoryManagerWhenCreatingImageFromSharedHandleThenReturnSuccess) {
    cl_mem_flags flags{};
    cl_int retVal{};
    const auto format = getValidImageFormat();
    const auto imageDesc = getValidImageDesc();
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                               &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto renderSize = image->getGraphicsAllocation(device->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo->getSizeAllocation();
    size_t expectedSize = imageDesc.image_row_pitch * imageDesc.image_height;
    EXPECT_GE(renderSize, expectedSize);
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
    EXPECT_EQ(GMM_FORMAT_R16G16_FLOAT_TYPE, image->getSurfaceFormatInfo().surfaceFormat.gmmSurfaceFormat);
    EXPECT_EQ(GFX3DSTATE_SURFACEFORMAT_R16G16_FLOAT, image->getSurfaceFormatInfo().surfaceFormat.genxSurfaceFormat);
}

template <bool pageTableManagerSupported>
class MockProductHelper : public ProductHelperHw<IGFX_UNKNOWN> {
  public:
    bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const override {
        return pageTableManagerSupported;
    }
};

struct MemoryManagerReturningCompressedAllocations : UnifiedSharingMockMemoryManager<true> {
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        auto allocation = UnifiedSharingMockMemoryManager<true>::createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);

        auto gmm = allocation->getDefaultGmm();
        auto mockGmmResourceInfo = std::make_unique<MockGmmResourceInfo>(gmm->gmmResourceInfo->peekGmmResourceInfo());
        mockGmmResourceInfo->mockResourceCreateParams.Type = RESOURCE_1D;
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
    using ProductHelperNotSupportingPageTableManager = MockProductHelper<false>;
    RAIIProductHelperFactory<ProductHelperNotSupportingPageTableManager> productHelperBackup{*this->context->getDevice(0)->getExecutionEnvironment()->rootDeviceEnvironments[0]};

    cl_mem_flags flags{};
    cl_int retVal{};
    const auto format = getValidImageFormat();
    const auto imageDesc = getValidImageDesc();
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                               &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(image->getGraphicsAllocation(device->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled());
    EXPECT_EQ(0u, memoryManager.calledMapAuxGpuVA);
}

HWTEST_F(UnifiedSharingImageTestsWithMemoryManager, givenCompressedImageAndPageTableManagerWhenCreatingUnifiedImageThenSetCorrespondingFieldInGmmBasedOnAuxGpuVaMappingResult) {
    MemoryManagerReturningCompressedAllocations memoryManager{};
    VariableBackup<MemoryManager *> memoryManagerBackup{&this->context->memoryManager, &memoryManager};
    using ProductHelperNotSupportingPageTableManager = MockProductHelper<true>;
    RAIIProductHelperFactory<ProductHelperNotSupportingPageTableManager> productHelperBackup{*this->context->getDevice(0)->getExecutionEnvironment()->rootDeviceEnvironments[0]};

    cl_mem_flags flags{};
    cl_int retVal{};
    const auto format = getValidImageFormat();
    const auto imageDesc = getValidImageDesc();

    memoryManager.resultOfMapAuxGpuVA = true;
    auto image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                               &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(memoryManager.resultOfMapAuxGpuVA, image->getGraphicsAllocation(device->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled());
    EXPECT_EQ(1u, memoryManager.calledMapAuxGpuVA);

    memoryManager.resultOfMapAuxGpuVA = false;
    image = std::unique_ptr<Image>(UnifiedImage::createSharedUnifiedImage(context.get(), flags, getValidUnifiedSharingDesc(),
                                                                          &format, &imageDesc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(memoryManager.resultOfMapAuxGpuVA, image->getGraphicsAllocation(device->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled());
    EXPECT_EQ(2u, memoryManager.calledMapAuxGpuVA);
}
