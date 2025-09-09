/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/aub_tests/fixtures/image_aub_fixture.h"

using namespace NEO;

struct WriteImageParams {
    cl_mem_object_type imageType;
    size_t offsets[3];
} writeImageParams[] = {
    {CL_MEM_OBJECT_IMAGE1D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE1D, {1u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE2D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE2D, {1u, 2u, 0u}},
    {CL_MEM_OBJECT_IMAGE3D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE3D, {1u, 2u, 3u}},
};

template <bool enableBlitter>
struct AUBWriteImage
    : public ImageAubFixture,
      public ::testing::WithParamInterface<std::tuple<uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, WriteImageParams>>,
      public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableFreeMemory.set(false);
        ImageAubFixture::setUp(enableBlitter);
    }

    void TearDown() override {
        dstImage.reset();

        ImageAubFixture::tearDown();
    }

    template <typename FamilyType>
    void testWriteImageUnaligned() {
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
        size_t inputRowPitch = testWidth * elementSize;
        size_t inputSlicePitch = inputRowPitch * testHeight;

        // Generate initial src memory but make it unaligned to page size
        auto srcMemoryAligned = alignedMalloc(4 + elementSize * numPixels, MemoryConstants::pageSize);
        auto srcMemoryUnaligned = ptrOffset(reinterpret_cast<uint8_t *>(srcMemoryAligned), 4);

        for (auto i = 0u; i < numPixels * elementSize; ++i) {
            srcMemoryUnaligned[i] = static_cast<uint8_t>(i);
        }

        auto retVal = CL_INVALID_VALUE;
        cl_mem_flags flags = 0;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        dstImage.reset(Image::create(
            context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            nullptr,
            retVal));
        ASSERT_NE(nullptr, dstImage.get());
        memset(dstImage->getCpuAddress(), 0xFF, dstImage->getSize()); // init image - avoid writeImage inside createImage (for tiled img)

        auto origin = std::get<2>(GetParam()).offsets;

        // Only draw 1/4 of the original image
        const size_t region[3] = {std::max(testWidth / 2, 1u),
                                  std::max(testHeight / 2, 1u),
                                  std::max(testDepth / 2, 1u)};

        retVal = pCmdQ->enqueueWriteImage(
            dstImage.get(),
            CL_TRUE,
            origin,
            region,
            inputRowPitch,
            inputSlicePitch,
            srcMemoryUnaligned,
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto readMemory = new uint8_t[dstImage->getSize()];
        size_t imgOrigin[] = {0, 0, 0};
        size_t imgRegion[] = {testWidth, testHeight, testDepth};
        retVal = pCmdQ->enqueueReadImage(dstImage.get(), CL_TRUE, imgOrigin, imgRegion, 0, 0, readMemory, nullptr, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pCmdQ->finish(false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto pDstMemory = readMemory;
        auto pSrcMemory = srcMemoryUnaligned;
        std::vector<uint8_t> referenceMemory(elementSize, 0xFF);

        auto rowPitch = dstImage->getHostPtrRowPitch();
        auto slicePitch = dstImage->getHostPtrSlicePitch();

        for (size_t z = 0; z < testDepth; ++z) {
            for (size_t y = 0; y < testHeight; ++y) {
                for (size_t x = 0; x < testWidth; ++x) {
                    auto pos = x * elementSize;
                    if (z >= origin[2] && z < (origin[2] + region[2]) &&
                        y >= origin[1] && y < (origin[1] + region[1]) &&
                        x >= origin[0] && x < (origin[0] + region[0])) {
                        // this texel should be updated
                        AUBCommandStreamFixture::expectMemory<FamilyType>(&pDstMemory[pos], pSrcMemory, elementSize);
                        pSrcMemory = ptrOffset(pSrcMemory, elementSize);
                    } else {
                        AUBCommandStreamFixture::expectMemory<FamilyType>(&pDstMemory[pos], referenceMemory.data(), elementSize);
                    }
                }
                pDstMemory = ptrOffset(pDstMemory, rowPitch);
                if (y >= origin[1] && y < origin[1] + region[1] &&
                    z >= origin[2] && z < origin[2] + region[2]) {
                    pSrcMemory = ptrOffset(pSrcMemory, inputRowPitch - (elementSize * region[0]));
                }
            }
            pDstMemory = ptrOffset(pDstMemory, slicePitch - (rowPitch * (testHeight > 0 ? testHeight : 1)));
            if (z >= origin[2] && z < origin[2] + region[2]) {
                pSrcMemory = ptrOffset(pSrcMemory, inputSlicePitch - (inputRowPitch * (region[1])));
            }
        }

        alignedFree(srcMemoryAligned);
        delete[] readMemory;
    }

    template <typename FamilyType>
    void testWriteImageMisaligned(size_t offset, size_t size, size_t pixelSize) {
        debugManager.flags.ForceLinearImages.set(true);

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

        auto srcMemoryAligned = alignedMalloc(4 + pixelSize * numPixels, MemoryConstants::cacheLineSize);
        memset(srcMemoryAligned, 0x0, 4 + pixelSize * numPixels);
        auto srcMemoryUnaligned = ptrOffset(reinterpret_cast<uint8_t *>(srcMemoryAligned), 4); // ensure non cacheline-aligned (but aligned to 4) hostPtr to create non-zerocopy image

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

        for (auto i = 0u; i < pixelSize * numPixels; ++i) {
            srcMemoryUnaligned[i] = static_cast<uint8_t>(i);
        }

        auto dstMemoryGPUPtr = reinterpret_cast<uint8_t *>(image->getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex())->getGpuAddress());

        const size_t origin[3] = {0, 1, 0};    // write first row
        const size_t region[3] = {size, 1, 1}; // write only "size" number of pixels
        size_t inputRowPitch = testWidth * pixelSize;
        size_t inputSlicePitch = inputRowPitch * testHeight;
        retVal = pCmdQ->enqueueWriteImage(
            image.get(),
            CL_TRUE,
            origin,
            region,
            inputRowPitch,
            inputSlicePitch,
            ptrOffset(srcMemoryUnaligned, offset),
            nullptr,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        pCmdQ->finish(false);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto imageRowPitch = image->getImageDesc().image_row_pitch;
        std::vector<uint8_t> referenceMemory(inputRowPitch * pixelSize, 0x0);

        AUBCommandStreamFixture::expectMemory<FamilyType>(dstMemoryGPUPtr, referenceMemory.data(), inputRowPitch);                                                                 // validate zero row is not written
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, imageRowPitch), &srcMemoryUnaligned[offset], size * pixelSize);                               // validate first row is written with correct data
        AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, imageRowPitch + size * pixelSize), referenceMemory.data(), inputRowPitch - size * pixelSize); // validate the remaining bytes of first row are not written
        for (uint32_t row = 2; row < testHeight; row++) {
            AUBCommandStreamFixture::expectMemory<FamilyType>(ptrOffset(dstMemoryGPUPtr, row * imageRowPitch), referenceMemory.data(), inputRowPitch); // validate the remaining rows are not written
        }

        alignedFree(srcMemoryAligned);
    }
    DebugManagerStateRestore restorer;
    std::unique_ptr<Image> dstImage;
};

using AUBWriteImageCCS = AUBWriteImage<false>;

HWTEST2_F(AUBWriteImageCCS, GivenMisalignedHostPtrWhenWritingImageThenExpectationsAreMet, ImagesSupportedMatcher) {
    const std::vector<size_t> pixelSizes = {1, 2, 4};
    const std::vector<size_t> offsets = {0, 4, 8, 12};
    const std::vector<size_t> sizes = {3, 2, 1};

    for (auto pixelSize : pixelSizes) {
        for (auto offset : offsets) {
            for (auto size : sizes) {
                testWriteImageMisaligned<FamilyType>(offset, size, pixelSize);
            }
        }
    }
}

HWTEST2_P(AUBWriteImageCCS, GivenUnalignedMemoryWhenWritingImageThenExpectationsAreMet, ImagesSupportedMatcher) {
    testWriteImageUnaligned<FamilyType>();
}

INSTANTIATE_TEST_SUITE_P(AUBWriteImage_simple, AUBWriteImageCCS,
                         ::testing::Combine(::testing::Values( // formats
                                                CL_UNORM_INT8, CL_SIGNED_INT16,
                                                CL_UNSIGNED_INT32, CL_HALF_FLOAT,
                                                CL_FLOAT),
                                            ::testing::Values( // channels
                                                CL_R, CL_RG, CL_RGBA),
                                            ::testing::ValuesIn(writeImageParams)));

using AUBWriteImageBCS = AUBWriteImage<true>;

HWTEST2_F(AUBWriteImageBCS, GivenMisalignedHostPtrWhenWritingImageWithBlitterEnabledThenExpectationsAreMet, ImagesSupportedMatcher) {
    const std::vector<size_t> pixelSizes = {1, 2, 4};
    const std::vector<size_t> offsets = {0, 4, 8, 12};
    const std::vector<size_t> sizes = {3, 2, 1};

    for (auto pixelSize : pixelSizes) {
        for (auto offset : offsets) {
            for (auto size : sizes) {
                testWriteImageMisaligned<FamilyType>(offset, size, pixelSize);
                ASSERT_EQ(pCmdQ->peekLatestSentEnqueueOperation(), EnqueueProperties::Operation::blit);
            }
        }
    }
}

HWTEST2_P(AUBWriteImageBCS, GivenUnalignedMemoryWhenWritingImageWithBlitterEnabledThenExpectationsAreMet, ImagesSupportedMatcher) {
    auto &productHelper = pCmdQ->getDevice().getProductHelper();
    if (std::get<2>(GetParam()).imageType == CL_MEM_OBJECT_IMAGE3D &&
        !(productHelper.isTile64With3DSurfaceOnBCSSupported(*defaultHwInfo))) {
        GTEST_SKIP();
    }

    testWriteImageUnaligned<FamilyType>();
    ASSERT_EQ(pCmdQ->peekLatestSentEnqueueOperation(), EnqueueProperties::Operation::blit);
}

INSTANTIATE_TEST_SUITE_P(AUBWriteImage_simple, AUBWriteImageBCS,
                         ::testing::Combine(::testing::Values( // formats
                                                CL_UNORM_INT8, CL_SIGNED_INT16,
                                                CL_UNSIGNED_INT32, CL_HALF_FLOAT,
                                                CL_FLOAT),
                                            ::testing::Values( // channels
                                                CL_R, CL_RG, CL_RGBA),
                                            ::testing::ValuesIn(writeImageParams)));
