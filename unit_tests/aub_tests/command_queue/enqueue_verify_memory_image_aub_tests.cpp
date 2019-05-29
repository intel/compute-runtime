/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "runtime/api/api.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

struct TestOffset {
    size_t offset[3];
};

struct VerifyMemoryImageHw
    : public CommandEnqueueAUBFixture,
      public ::testing::TestWithParam<TestOffset> {

    void SetUp() override {
        CommandEnqueueAUBFixture::SetUp();
    }

    void TearDown() override {
        CommandEnqueueAUBFixture::TearDown();
    }
};

TestOffset testInput[] = {
    {{0, 0, 0}},
    {{1, 2, 3}},
    {{3, 2, 1}},
    {{5, 5, 5}}};

HWTEST_P(VerifyMemoryImageHw, givenDifferentImagesWhenValidatingMemoryThenSuccessIsReturned) {
    cl_image_format imageFormat;
    cl_image_desc imageDesc;

    // clang-format off
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT32;
    imageFormat.image_channel_order     = CL_RGBA;

    imageDesc.image_type        = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width       = 10;
    imageDesc.image_height      = 19;
    imageDesc.image_depth       = 7;
    imageDesc.image_array_size  = 1;
    imageDesc.image_row_pitch   = 0;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_mip_levels    = 0;
    imageDesc.num_samples       = 0;
    imageDesc.mem_object        = NULL;
    // clang-format on

    // data per channel multplied by number of channels
    size_t elementSize = 16;

    cl_mem_flags flags = CL_MEM_READ_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto retVal = CL_INVALID_VALUE;
    std::unique_ptr<Image> image(Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal));
    EXPECT_NE(nullptr, image);

    auto sizeMemory = image->getSize();
    EXPECT_GT(sizeMemory, 0u);

    std::unique_ptr<uint8_t> srcMemory(new uint8_t[elementSize]);
    memset(srcMemory.get(), 0xAB, elementSize);
    memset(image->getCpuAddress(), 0xAB, sizeMemory);

    const size_t *origin = GetParam().offset;

    const size_t region[] = {
        imageDesc.image_width - origin[0],
        imageDesc.image_height - origin[1],
        imageDesc.image_depth - origin[2]};

    uint32_t fillValues[] = {0x3f800000, 0x00000000, 0x3f555555, 0x3f2aaaaa};
    retVal = pCmdQ->enqueueFillImage(
        image.get(),
        fillValues,
        origin,
        region,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t imgOrigin[] = {0, 0, 0};
    size_t imgRegion[] = {imageDesc.image_width, imageDesc.image_height, imageDesc.image_depth};

    size_t mappedRowPitch;
    size_t mappedSlicePitch;
    auto mappedAddress = clEnqueueMapImage(pCmdQ, image.get(), CL_TRUE, CL_MAP_READ, imgOrigin, imgRegion,
                                           &mappedRowPitch, &mappedSlicePitch, 0, nullptr, nullptr, &retVal);
    auto pImageData = reinterpret_cast<uint8_t *>(mappedAddress);
    for (size_t z = 0; z < imageDesc.image_depth; ++z) {
        for (size_t y = 0; y < imageDesc.image_height; ++y) {
            for (size_t x = 0; x < imageDesc.image_width; ++x) {
                void *validData = srcMemory.get();
                void *invalidData = fillValues;

                if (z >= origin[2] && z < (origin[2] + region[2]) &&
                    y >= origin[1] && y < (origin[1] + region[1]) &&
                    x >= origin[0] && x < (origin[0] + region[0])) {
                    std::swap(validData, invalidData);
                }

                retVal = clEnqueueVerifyMemoryINTEL(pCmdQ, &pImageData[x * elementSize], validData, elementSize, CL_MEM_COMPARE_EQUAL);
                EXPECT_EQ(CL_SUCCESS, retVal);

                if (UnitTestHelper<FamilyType>::isExpectMemoryNotEqualSupported()) {
                    retVal = clEnqueueVerifyMemoryINTEL(pCmdQ, &pImageData[x * elementSize], invalidData, elementSize,
                                                        CL_MEM_COMPARE_NOT_EQUAL);
                    EXPECT_EQ(CL_SUCCESS, retVal);
                }
            }
            pImageData = ptrOffset(pImageData, mappedRowPitch);
        }
        pImageData = ptrOffset(pImageData, mappedSlicePitch - (mappedRowPitch * imageDesc.image_height));
    }
}

INSTANTIATE_TEST_CASE_P(VerifyMemoryImage,
                        VerifyMemoryImageHw,
                        ::testing::ValuesIn(testInput));
