/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/aub_tests/fixtures/image_aub_fixture.h"

using namespace NEO;

template <bool enableBlitter>
struct AUBCopyImage
    : public ImageAubFixture,
      public ::testing::WithParamInterface<std::tuple<uint32_t, uint32_t>>,
      public ::testing::Test {
    void SetUp() override {
        ImageAubFixture::SetUp(enableBlitter);
    }

    void TearDown() override {
        srcImage.reset();
        dstImage.reset();

        ImageAubFixture::TearDown();
    }

    template <typename FamilyType>
    void runAubTest() {
        const size_t testImageDimensions = 4;
        cl_float srcMemory[testImageDimensions * testImageDimensions] = {
            1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 0.5f, 1.5f, 2.5f,
            3.5f, 4.5f, 5.5f, 6.5f};

        cl_float origValue = -1.0f;
        cl_float dstMemory[testImageDimensions * testImageDimensions] = {
            origValue, origValue, origValue, origValue,
            origValue, origValue, origValue, origValue,
            origValue, origValue, origValue, origValue,
            origValue, origValue, origValue, origValue};

        cl_image_format imageFormat;
        cl_image_desc imageDesc;
        cl_mem_flags flags = CL_MEM_COPY_HOST_PTR;
        // clang-format off
        imageFormat.image_channel_data_type = CL_FLOAT;
        imageFormat.image_channel_order     = CL_R;

        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = testImageDimensions;
        imageDesc.image_height      = testImageDimensions;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object = NULL;
        // clang-format on
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, pClDevice->getHardwareInfo().capabilityTable.supportsOcl21Features);
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

        dstImage.reset(Image::create(
            context,
            ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
            flags,
            0,
            surfaceFormat,
            &imageDesc,
            dstMemory,
            retVal));
        ASSERT_NE(nullptr, dstImage.get());

        size_t srcOffset = std::get<0>(GetParam());
        size_t dstOffset = std::get<1>(GetParam());
        size_t srcOrigin[3] = {srcOffset, srcOffset, 0};
        size_t dstOrigin[3] = {dstOffset, dstOffset, 0};
        // Only draw 1/4 of the original image
        const size_t region[3] = {
            testImageDimensions / 2,
            testImageDimensions / 2,
            1};

        retVal = pCmdQ->enqueueCopyImage(
            srcImage.get(),
            dstImage.get(),
            srcOrigin,
            dstOrigin,
            region,
            0,
            nullptr,
            nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto dstOutMemory = new cl_float[dstImage->getSize()];

        size_t imgOrigin[] = {0, 0, 0};
        size_t imgRegion[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_depth};

        retVal = pCmdQ->enqueueReadImage(dstImage.get(), CL_FALSE, imgOrigin, imgRegion, 0, 0, dstOutMemory, nullptr, 0, nullptr, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        retVal = pCmdQ->finish();
        EXPECT_EQ(CL_SUCCESS, retVal);

        // Offset the source memory
        auto pSrcMemory = ptrOffset(srcMemory, (srcOffset * testImageDimensions + srcOffset) * sizeof(origValue));

        // Since the driver allocated his own memory, we need to use that for verification
        auto pDstMemory = static_cast<float *>(dstOutMemory);

        if (dstOffset > 0) {
            // Add expectations for rows that should be unmodified
            AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, dstMemory, dstOffset * testImageDimensions * sizeof(origValue));
            pDstMemory = ptrOffset(pDstMemory, dstOffset * testImageDimensions * sizeof(origValue));
        }

        for (size_t row = 0; row < region[1]; ++row) {
            if (dstOffset > 0) {
                size_t length = dstOffset * sizeof(origValue);
                AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, dstMemory, length);
                pDstMemory = ptrOffset(pDstMemory, length);
            }

            size_t length = region[0] * sizeof(origValue);
            AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, pSrcMemory, length);
            pDstMemory = ptrOffset(pDstMemory, length);

            length = (testImageDimensions - region[0] - dstOffset) * sizeof(origValue);
            AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, dstMemory, length);
            pDstMemory = ptrOffset(pDstMemory, length);

            pSrcMemory = ptrOffset(pSrcMemory, testImageDimensions * sizeof(origValue));
        }

        size_t remainingRows = testImageDimensions - region[1] - dstOffset;
        while (remainingRows > 0) {
            size_t length = testImageDimensions * sizeof(origValue);
            AUBCommandStreamFixture::expectMemory<FamilyType>(pDstMemory, dstMemory, length);
            pDstMemory = ptrOffset(pDstMemory, length);
            --remainingRows;
        }

        delete[] dstOutMemory;
    }

    std::unique_ptr<Image> srcImage;
    std::unique_ptr<Image> dstImage;
};

using AUBCopyImageCCS = AUBCopyImage<false>;

HWTEST2_P(AUBCopyImageCCS, WhenCopyingThenExpectationsMet, ImagesSupportedMatcher) {
    runAubTest<FamilyType>();
}

INSTANTIATE_TEST_CASE_P(AUBCopyImage_simple,
                        AUBCopyImageCCS,
                        ::testing::Combine(
                            ::testing::Values( // srcOffset
                                0u, 1u, 2u),
                            ::testing::Values( // dstOffset
                                0u, 1u, 2u)));

using AUBCopyImageBCS = AUBCopyImage<true>;

HWTEST2_P(AUBCopyImageBCS, WhenCopyingWithBlitterEnabledThenExpectationsMet, ImagesSupportedMatcher) {
    runAubTest<FamilyType>();
    ASSERT_EQ(pCmdQ->peekLatestSentEnqueueOperation(), EnqueueProperties::Operation::Blit);
}

INSTANTIATE_TEST_CASE_P(AUBCopyImage_simple,
                        AUBCopyImageBCS,
                        ::testing::Combine(
                            ::testing::Values( // srcOffset
                                0u, 1u, 2u),
                            ::testing::Values( // dstOffset
                                0u, 1u, 2u)));
