/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/aub_tests/fixtures/image_aub_fixture.h"

using namespace NEO;

struct ReadImageParams {
    cl_mem_object_type imageType;
    size_t offsets[3];
} readImageParams[] = {
    {CL_MEM_OBJECT_IMAGE1D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE1D, {1u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE2D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE2D, {1u, 2u, 0u}},
    {CL_MEM_OBJECT_IMAGE3D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE3D, {1u, 2u, 3u}},
};

template <bool enableBlitter>
struct AUBReadImage
    : public ImageAubFixture,
      public ::testing::WithParamInterface<std::tuple<uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, ReadImageParams>>,
      public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableFreeMemory.set(false);
        ImageAubFixture::setUp(enableBlitter);
    }

    void TearDown() override {
        srcImage.reset();

        ImageAubFixture::tearDown();
    }

    template <typename FamilyType>
    void testReadImageUnaligned() {
        const auto testWidth = 5u;
        const auto testHeight = std::get<2>(GetParam()).imageType != CL_MEM_OBJECT_IMAGE1D ? 5u : 1u;
        const auto testDepth = std::get<2>(GetParam()).imageType == CL_MEM_OBJECT_IMAGE3D ? 5u : 1u;
        auto numPixels = testWidth * testHeight * testDepth;

        cl_image_format imageFormat;
        cl_image_desc imageDesc;
        // clang-format off
        imageFormat.image_channel_data_type = std::get<0>(GetParam());
        imageFormat.image_channel_order     = std::get<1>(GetParam());

        imageDesc.image_type        = std::get<2>(GetParam()).imageType;
        imageDesc.image_width       = testWidth;
        imageDesc.image_height      = testHeight;
        imageDesc.image_depth       = testDepth;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object        = NULL;
        // clang-format on

        auto perChannelDataSize = 0u;
        switch (imageFormat.image_channel_data_type) {
        case CL_UNORM_INT8:
            perChannelDataSize = 1u;
            break;
        case CL_SIGNED_INT16:
        case CL_HALF_FLOAT:
            perChannelDataSize = 2u;
            break;
        case CL_UNSIGNED_INT32:
        case CL_FLOAT:
            perChannelDataSize = 4u;
            break;
        }

        auto numChannels = 0u;
        switch (imageFormat.image_channel_order) {
        case CL_R:
            numChannels = 1u;
            break;
        case CL_RG:
            numChannels = 2u;
            break;
        case CL_RGBA:
            numChannels = 4u;
            break;
        }
        size_t elementSize = perChannelDataSize * numChannels;
        size_t rowPitch = testWidth * elementSize;
        size_t slicePitch = rowPitch * testHeight;

        // Generate initial dst memory but make it unaligned to page size
        auto dstMemoryAligned = alignedMalloc(4 + elementSize * numPixels, MemoryConstants::pageSize);
        auto dstMemoryUnaligned = ptrOffset(reinterpret_cast<uint8_t *>(dstMemoryAligned), 4);

        auto sizeMemory = testWidth * alignUp(testHeight, 4) * testDepth * elementSize;
        auto srcMemoryAligned = alignedMalloc(sizeMemory, 4);
        auto srcMemory = reinterpret_cast<uint8_t *>(srcMemoryAligned);
        for (auto i = 0u; i < sizeMemory; ++i) {
            srcMemory[i] = static_cast<uint8_t>(i);
        }

        memset(dstMemoryUnaligned, 0xFF, numPixels * elementSize);

        cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        auto retVal = CL_INVALID_VALUE;
        srcImage.reset(Image::create(
            context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            srcMemory,
            retVal));
        ASSERT_NE(nullptr, srcImage.get());

        auto origin = std::get<2>(GetParam()).offsets;

        // Only draw 1/4 of the original image
        const size_t region[3] = {std::max(testWidth / 2, 1u),
                                  std::max(testHeight / 2, 1u),
                                  std::max(testDepth / 2, 1u)};

        retVal = pCmdQ->enqueueReadImage(
            srcImage.get(),
            CL_TRUE,
            origin,
            region,
            rowPitch,
            slicePitch,
            dstMemoryUnaligned,
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pCmdQ->finish(false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto imageMemory = srcMemory;
        auto memoryPool = srcImage->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex())->getMemoryPool();
        bool isGpuCopy = srcImage->isTiledAllocation() || !MemoryPoolHelper::isSystemMemoryPool(memoryPool);
        if (!isGpuCopy) {
            imageMemory = reinterpret_cast<uint8_t *>(srcImage->getCpuAddress());
        }

        auto offset = (origin[2] * testWidth * testHeight + origin[1] * testWidth + origin[0]) * elementSize;
        auto pSrcMemory = ptrOffset(imageMemory, offset);
        auto pDstMemory = dstMemoryUnaligned;

        for (auto depth = origin[2] + 1; depth < (origin[2] + region[2]); ++depth) {

            for (size_t row = 0; row < region[1]; ++row) {

                size_t length = region[0] * elementSize;
                AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, pSrcMemory, length);
                pDstMemory = ptrOffset(pDstMemory, length);

                length = (testWidth - region[0]) * elementSize;
                AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, pDstMemory, length);
                pDstMemory = ptrOffset(pDstMemory, length);

                pSrcMemory = ptrOffset(pSrcMemory, testWidth * elementSize);
            }

            size_t remainingRows = testHeight - region[1];
            while (remainingRows > 0) {
                size_t length = testHeight * elementSize;
                AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, pDstMemory, length);
                pDstMemory = ptrOffset(pDstMemory, length);
                --remainingRows;
            }

            pDstMemory = ptrOffset(dstMemoryUnaligned, testWidth * testHeight * elementSize);
        }

        alignedFree(dstMemoryAligned);
        alignedFree(srcMemoryAligned);
    }

    template <typename FamilyType>
    void testReadImageMisaligned(size_t offset, size_t size, size_t pixelSize) {
        const size_t testWidth = 14 / pixelSize;
        const size_t testHeight = 4;
        const size_t testDepth = 1;
        auto numPixels = testWidth * testHeight * testDepth;

        cl_image_format imageFormat;
        cl_image_desc imageDesc;
        imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
        switch (pixelSize) {
        case 1:
            imageFormat.image_channel_order = CL_R;
            break;
        case 2:
            imageFormat.image_channel_order = CL_RG;
            break;
        case 3:
            ASSERT_TRUE(false);
            break;
        case 4:
            imageFormat.image_channel_order = CL_RGBA;
            break;
        }
        imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width = testWidth;
        imageDesc.image_height = testHeight;
        imageDesc.image_depth = testDepth;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;

        auto dstMemoryAligned = alignedMalloc(pixelSize * numPixels, MemoryConstants::cacheLineSize);
        memset(dstMemoryAligned, 0x0, pixelSize * numPixels);

        auto srcMemoryAligned = alignedMalloc(4 + pixelSize * numPixels, 4);
        auto srcMemoryUnaligned = reinterpret_cast<uint8_t *>(ptrOffset(srcMemoryAligned, 4));
        for (auto i = 0u; i < pixelSize * numPixels; ++i) {
            srcMemoryUnaligned[i] = static_cast<uint8_t>(i);
        }

        cl_mem_flags flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, pClDevice->getHardwareInfo().capabilityTable.supportsOcl21Features);
        auto retVal = CL_INVALID_VALUE;
        auto image = std::unique_ptr<Image>(Image::create(
            context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            srcMemoryUnaligned,
            retVal));
        ASSERT_NE(nullptr, image);
        EXPECT_FALSE(image->isMemObjZeroCopy());

        auto csr = enableBlitter ? pCmdQ->getBcsCommandStreamReceiver(aub_stream::EngineType::ENGINE_BCS) : pCommandStreamReceiver;
        auto graphicsAllocation = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, pixelSize * numPixels}, dstMemoryAligned);
        csr->makeResidentHostPtrAllocation(graphicsAllocation);
        csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(graphicsAllocation), TEMPORARY_ALLOCATION);
        auto dstMemoryGPUPtr = reinterpret_cast<uint8_t *>(graphicsAllocation->getGpuAddress());

        const size_t origin[3] = {0, 1, 0};
        const size_t region[3] = {size, 1, 1};
        size_t inputRowPitch = testWidth * pixelSize;
        size_t inputSlicePitch = inputRowPitch * testHeight;
        retVal = pCmdQ->enqueueReadImage(
            image.get(),
            CL_FALSE,
            origin,
            region,
            inputRowPitch,
            inputSlicePitch,
            ptrOffset(dstMemoryAligned, offset),
            nullptr,
            0,
            nullptr,
            nullptr);

        EXPECT_EQ(CL_SUCCESS, retVal);
        pCmdQ->finish(false);

        std::vector<uint8_t> referenceMemory(pixelSize * numPixels, 0x0);

        AUBCommandStreamFixture::expectMemory<FamilyType>(dstMemoryGPUPtr, referenceMemory.data(), offset);
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset), &srcMemoryUnaligned[inputRowPitch * origin[1]], size * pixelSize);
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, offset + size * pixelSize), referenceMemory.data(), pixelSize * numPixels - offset - size * pixelSize);

        alignedFree(dstMemoryAligned);
        alignedFree(srcMemoryAligned);
    }
    DebugManagerStateRestore restorer;
    std::unique_ptr<Image> srcImage;
};

using AUBReadImageCCS = AUBReadImage<false>;

HWTEST2_F(AUBReadImageCCS, GivenMisalignedHostPtrWhenReadingImageThenExpectationsAreMet, ImagesSupportedMatcher) {
    const std::vector<size_t> pixelSizes = {1, 2, 4};
    const std::vector<size_t> offsets = {0, 4, 8, 12};
    const std::vector<size_t> sizes = {3, 2, 1};

    for (auto pixelSize : pixelSizes) {
        for (auto offset : offsets) {
            for (auto size : sizes) {
                testReadImageMisaligned<FamilyType>(offset, size, pixelSize);
            }
        }
    }
}

HWTEST2_P(AUBReadImageCCS, GivenUnalignedMemoryWhenReadingImageThenExpectationsAreMet, ImagesSupportedMatcher) {
    testReadImageUnaligned<FamilyType>();
}

INSTANTIATE_TEST_SUITE_P(
    AUBReadImage_simple, AUBReadImageCCS,
    ::testing::Combine(::testing::Values( // formats
                           CL_UNORM_INT8, CL_SIGNED_INT16, CL_UNSIGNED_INT32,
                           CL_HALF_FLOAT, CL_FLOAT),
                       ::testing::Values( // channels
                           CL_R, CL_RG, CL_RGBA),
                       ::testing::ValuesIn(readImageParams)));

using AUBReadImageBCS = AUBReadImage<true>;

HWTEST2_F(AUBReadImageBCS, GivenMisalignedHostPtrWhenReadingImageWithBlitterEnabledThenExpectationsAreMet, ImagesSupportedMatcher) {
    const std::vector<size_t> pixelSizes = {1, 2, 4};
    const std::vector<size_t> offsets = {0, 4, 8, 12};
    const std::vector<size_t> sizes = {3, 2, 1};

    for (auto pixelSize : pixelSizes) {
        for (auto offset : offsets) {
            for (auto size : sizes) {
                testReadImageMisaligned<FamilyType>(offset, size, pixelSize);
                ASSERT_EQ(pCmdQ->peekLatestSentEnqueueOperation(), EnqueueProperties::Operation::blit);
            }
        }
    }
}

HWTEST2_P(AUBReadImageBCS, GivenUnalignedMemoryWhenReadingImageWithBlitterEnabledThenExpectationsAreMet, ImagesSupportedMatcher) {

    auto &productHelper = pCmdQ->getDevice().getProductHelper();
    if (std::get<2>(GetParam()).imageType == CL_MEM_OBJECT_IMAGE3D &&
        !(productHelper.isTile64With3DSurfaceOnBCSSupported(*defaultHwInfo))) {
        GTEST_SKIP();
    }

    testReadImageUnaligned<FamilyType>();
    ASSERT_EQ(pCmdQ->peekLatestSentEnqueueOperation(), EnqueueProperties::Operation::blit);
}

INSTANTIATE_TEST_SUITE_P(
    AUBReadImage_simple, AUBReadImageBCS,
    ::testing::Combine(::testing::Values( // formats
                           CL_UNORM_INT8, CL_SIGNED_INT16, CL_UNSIGNED_INT32,
                           CL_HALF_FLOAT, CL_FLOAT),
                       ::testing::Values( // channels
                           CL_R, CL_RG, CL_RGBA),
                       ::testing::ValuesIn(readImageParams)));
