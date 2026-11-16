/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/test_traits.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace L0 {
namespace ult {

template <bool useLocalMemory>
struct CompressionL0AubFixture : public AUBFixtureL0,
                                 public ::testing::Test,
                                 public ::testing::WithParamInterface<uint32_t> {
    void SetUp() override {
        auto &ftrTable = NEO::defaultHwInfo->featureTable;
        if ((!ftrTable.flags.ftrFlatPhysCCS) ||
            (!ftrTable.flags.ftrLocalMemory && useLocalMemory)) {
            GTEST_SKIP();
        }

        debugManager.flags.RenderCompressedBuffersEnabled.set(true);
        debugManager.flags.RenderCompressedImagesEnabled.set(true);
        debugManager.flags.OverrideBufferSuitableForRenderCompression.set(1);
        debugManager.flags.EnableLocalMemory.set(useLocalMemory);
        debugManager.flags.NodeOrdinal.set(GetParam());
        debugManager.flags.EnableDeviceUsmAllocationPool.set(0);

        NEO::MockExecutionEnvironment mockExecutionEnvironment{};
        auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<NEO::GfxCoreHelper>();
        const auto expectedEngine = static_cast<aub_stream::EngineType>(GetParam());
        const auto engines = gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment);
        if (std::none_of(engines.begin(), engines.end(), [expectedEngine](const auto &engine) { return engine.first == expectedEngine; })) {
            GTEST_SKIP();
        }

        AUBFixtureL0::setUp();
    }

    void TearDown() override {
        for (auto image : this->images) {
            zeImageDestroy(image);
        }
        AUBFixtureL0::tearDown();
    }

    void *allocDeviceWithCompressionHint(size_t size, ze_memory_compression_hints_ext_flags_t compressionFlags) {
        ze_memory_compression_hints_ext_desc_t compressionHint = {ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC};
        compressionHint.flags = compressionFlags;
        ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        deviceDesc.pNext = &compressionHint;
        void *ptr = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemAllocDevice(this->context->toHandle(), &deviceDesc, size, MemoryConstants::pageSize64k, this->device->toHandle(), &ptr));
        if (ptr) {
            this->usmAllocations.push_back(ptr);
        }
        return ptr;
    }

    NEO::GraphicsAllocation *getAllocation(void *ptr) {
        auto allocData = this->driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
        EXPECT_NE(nullptr, allocData);
        if (!allocData) {
            return nullptr;
        }
        return allocData->gpuAllocations.getDefaultGraphicsAllocation();
    }

    ze_image_handle_t createImage(const ze_image_desc_t &imageDesc) {
        ze_image_handle_t image = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeImageCreate(this->context->toHandle(), this->device->toHandle(), &imageDesc, &image));
        if (image) {
            this->images.push_back(image);
        }
        return image;
    }

    template <typename FamilyType>
    void givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect();
    template <typename FamilyType>
    void givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect();
    template <typename FamilyType>
    void givenCompressedImageWhenReadingThenResultsAreCorrect();

    std::vector<ze_image_handle_t> images;
};

template <bool useLocalMemory>
template <typename FamilyType>
void CompressionL0AubFixture<useLocalMemory>::givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect() {
    constexpr size_t bufferSize = 2048;
    auto writePattern = static_cast<uint8_t *>(this->allocateHostMemory(bufferSize, 1));
    std::fill(writePattern, writePattern + bufferSize, 1);

    auto compressedBuffer = allocDeviceWithCompressionHint(bufferSize, ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED);
    auto compressedAllocation = getAllocation(compressedBuffer);
    ASSERT_NE(nullptr, compressedAllocation);
    ASSERT_NE(nullptr, compressedAllocation->getDefaultGmm());
    EXPECT_NE(nullptr, compressedAllocation->getDefaultGmm()->gmmResourceInfo->peekHandle());
    EXPECT_TRUE(compressedAllocation->getDefaultGmm()->isCompressionEnabled());
    if (useLocalMemory) {
        EXPECT_EQ(NEO::MemoryPool::localMemory, compressedAllocation->getMemoryPool());
    } else {
        EXPECT_EQ(NEO::MemoryPool::systemCpuInaccessible, compressedAllocation->getMemoryPool());
    }

    auto notCompressedBuffer = allocDeviceWithCompressionHint(bufferSize, ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED);
    auto notCompressedAllocation = getAllocation(notCompressedBuffer);
    ASSERT_NE(nullptr, notCompressedAllocation);
    EXPECT_FALSE(notCompressedAllocation->isCompressionEnabled());

    auto cmdListHandle = this->commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, compressedBuffer, writePattern, bufferSize, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(cmdListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, notCompressedBuffer, compressedBuffer, bufferSize, nullptr, 0, nullptr));
    executeAndSynchronize();

    this->template expectNotEqualMemory<FamilyType>(compressedBuffer, writePattern, bufferSize);
    this->template expectMemory<FamilyType>(notCompressedBuffer, writePattern, bufferSize);
}

template <bool useLocalMemory>
template <typename FamilyType>
void CompressionL0AubFixture<useLocalMemory>::givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect() {
    constexpr uint32_t imageWidth = 16;
    constexpr uint32_t imageHeight = 16;
    constexpr size_t bufferSize = 64 * MemoryConstants::kiloByte;
    constexpr size_t perChannelDataSize = sizeof(uint32_t);
    constexpr size_t numChannels = 4;
    constexpr size_t imageSize = imageWidth * imageHeight * perChannelDataSize * numChannels;

    auto writePattern = static_cast<uint8_t *>(this->allocateHostMemory(bufferSize, 1));
    std::fill(writePattern, writePattern + bufferSize, 1);
    auto destMemory = static_cast<uint8_t *>(this->allocateHostMemory(imageSize, 1));
    memset(destMemory, 0, imageSize);

    auto compressedBuffer = allocDeviceWithCompressionHint(bufferSize, ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED);
    auto cmdListHandle = this->commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, compressedBuffer, writePattern, bufferSize, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(cmdListHandle, nullptr, 0, nullptr));

    ze_image_pitched_exp_desc_t pitchedDesc = {ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC};
    pitchedDesc.ptr = compressedBuffer;

    ze_image_desc_t imageDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    imageDesc.pNext = &pitchedDesc;
    imageDesc.flags = ZE_IMAGE_FLAG_KERNEL_WRITE;
    imageDesc.type = ZE_IMAGE_TYPE_2D;
    imageDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT,
                        ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                        ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};
    imageDesc.width = imageWidth;
    imageDesc.height = imageHeight;
    imageDesc.depth = 1;
    imageDesc.arraylevels = 0;
    imageDesc.miplevels = 0;

    auto compressedImage = createImage(imageDesc);
    ASSERT_NE(nullptr, compressedImage);

    ze_image_region_t region = {0, 0, 0, imageWidth, imageHeight, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdListHandle, destMemory, compressedImage, &region, nullptr, 0, nullptr));
    executeAndSynchronize();

    this->template expectMemory<FamilyType>(destMemory, writePattern, imageSize);

    auto bufferAllocation = getAllocation(compressedBuffer);
    ASSERT_NE(nullptr, bufferAllocation);
    ASSERT_NE(nullptr, bufferAllocation->getDefaultGmm());
    EXPECT_TRUE(bufferAllocation->getDefaultGmm()->isCompressionEnabled());
    auto imageAllocation = Image::fromHandle(compressedImage)->getAllocation();
    ASSERT_NE(nullptr, imageAllocation);
    ASSERT_NE(nullptr, imageAllocation->getDefaultGmm());
    EXPECT_TRUE(imageAllocation->getDefaultGmm()->isCompressionEnabled());

    this->template expectNotEqualMemory<FamilyType>(compressedBuffer, writePattern, bufferSize);
}

template <bool useLocalMemory>
template <typename FamilyType>
void CompressionL0AubFixture<useLocalMemory>::givenCompressedImageWhenReadingThenResultsAreCorrect() {
    constexpr uint32_t imageWidth = 8;
    constexpr uint32_t imageHeight = 4;
    constexpr size_t perChannelDataSize = sizeof(float);
    constexpr size_t numChannels = 4;
    constexpr size_t imageSize = imageWidth * imageHeight * perChannelDataSize * numChannels;
    constexpr size_t rowSize = imageSize / imageHeight;

    auto srcMemory = static_cast<float *>(this->allocateHostMemory(imageSize, 1));
    auto destMemory = static_cast<float *>(this->allocateHostMemory(imageSize, 1));
    memset(destMemory, 0, imageSize);

    const float row[rowSize / sizeof(float)] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                                                1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                                                1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                                                1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    for (uint32_t height = 0; height < imageHeight; height++) {
        memcpy(ptrOffset(srcMemory, height * rowSize), row, rowSize);
    }

    ze_image_desc_t imageDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
    imageDesc.flags = ZE_IMAGE_FLAG_KERNEL_WRITE;
    imageDesc.type = ZE_IMAGE_TYPE_2D;
    imageDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                        ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                        ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};
    imageDesc.width = imageWidth;
    imageDesc.height = imageHeight;
    imageDesc.depth = 1;
    imageDesc.arraylevels = 0;
    imageDesc.miplevels = 0;

    auto srcImage = createImage(imageDesc);
    ASSERT_NE(nullptr, srcImage);

    auto cmdListHandle = this->commandList->toHandle();
    ze_image_region_t region = {0, 0, 0, imageWidth, imageHeight, 1};
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyFromMemory(cmdListHandle, srcImage, srcMemory, &region, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendBarrier(cmdListHandle, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendImageCopyToMemory(cmdListHandle, destMemory, srcImage, &region, nullptr, 0, nullptr));
    executeAndSynchronize();

    this->template expectMemory<FamilyType>(destMemory, srcMemory, imageSize);

    auto imageAllocation = Image::fromHandle(srcImage)->getAllocation();
    ASSERT_NE(nullptr, imageAllocation);
    this->template expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(imageAllocation->getGpuAddress()), srcMemory, imageSize);
}

struct CompressionLocalAubsSupportedL0Matcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (NEO::HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::localMemCompressionAubsSupported;
        }
        return false;
    }
};

struct CompressionSystemAubsSupportedL0Matcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (NEO::HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::systemMemCompressionAubsSupported;
        }
        return false;
    }
};

template <PRODUCT_FAMILY... excludedProducts>
struct CompressionSystemAubsSupportedExceptL0Matcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return ((productFamily != excludedProducts) && ...) && CompressionSystemAubsSupportedL0Matcher::isMatched<productFamily>();
    }
};

using CompressionSystemBuffersL0Matcher = CompressionSystemAubsSupportedExceptL0Matcher<IGFX_LUNARLAKE>;
using CompressionSystemImageReadL0Matcher = CompressionSystemAubsSupportedExceptL0Matcher<IGFX_LUNARLAKE, IGFX_BMG>;

using CompressionLocalL0AubTests = CompressionL0AubFixture<true>;
HWTEST2_P(CompressionLocalL0AubTests, givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect, CompressionLocalAubsSupportedL0Matcher) {
    givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect<FamilyType>();
}
HWTEST2_P(CompressionLocalL0AubTests, givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect, CompressionLocalAubsSupportedL0Matcher) {
    givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect<FamilyType>();
}
HWTEST2_P(CompressionLocalL0AubTests, givenCompressedImageWhenReadingThenResultsAreCorrect, CompressionLocalAubsSupportedL0Matcher) {
    givenCompressedImageWhenReadingThenResultsAreCorrect<FamilyType>();
}

INSTANTIATE_TEST_SUITE_P(,
                         CompressionLocalL0AubTests,
                         ::testing::Values(aub_stream::ENGINE_RCS,
                                           aub_stream::ENGINE_CCS));

using CompressionSystemL0AubTests = CompressionL0AubFixture<false>;
HWTEST2_P(CompressionSystemL0AubTests, GENERATEONLY_givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect, CompressionSystemBuffersL0Matcher) {
    givenCompressedBuffersWhenWritingAndCopyingThenResultsAreCorrect<FamilyType>();
}
HWTEST2_P(CompressionSystemL0AubTests, GENERATEONLY_givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect, CompressionSystemBuffersL0Matcher) {
    givenCompressedImage2DFromBufferWhenItIsUsedThenDataIsCorrect<FamilyType>();
}
HWTEST2_P(CompressionSystemL0AubTests, givenCompressedImageWhenReadingThenResultsAreCorrect, CompressionSystemImageReadL0Matcher) {
    givenCompressedImageWhenReadingThenResultsAreCorrect<FamilyType>();
}

INSTANTIATE_TEST_SUITE_P(,
                         CompressionSystemL0AubTests,
                         ::testing::Values(aub_stream::ENGINE_RCS,
                                           aub_stream::ENGINE_CCS));

} // namespace ult
} // namespace L0
