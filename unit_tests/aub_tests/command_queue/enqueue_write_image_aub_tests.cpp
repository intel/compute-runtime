/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/aub_tests/command_queue/enqueue_read_write_image_aub_fixture.h"
#include "unit_tests/mocks/mock_context.h"

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

struct AUBWriteImage
    : public CommandDeviceFixture,
      public AUBCommandStreamFixture,
      public ::testing::WithParamInterface<std::tuple<uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, WriteImageParams>>,
      public ::testing::Test {
    typedef AUBCommandStreamFixture CommandStreamFixture;

    using AUBCommandStreamFixture::SetUp;

    void SetUp() override {
        CommandDeviceFixture::SetUp(cl_command_queue_properties(0));
        CommandStreamFixture::SetUp(pCmdQ);
        context = new MockContext(pDevice);
    }

    void TearDown() override {
        delete dstImage;
        delete context;
        CommandStreamFixture::TearDown();
        CommandDeviceFixture::TearDown();
    }

    MockContext *context;
    Image *dstImage = nullptr;
};

HWTEST_P(AUBWriteImage, simpleUnalignedMemory) {

    const unsigned int testWidth = 5;
    const unsigned int testHeight =
        std::get<2>(GetParam()).imageType != CL_MEM_OBJECT_IMAGE1D ? 5 : 1;
    const unsigned int testDepth =
        std::get<2>(GetParam()).imageType == CL_MEM_OBJECT_IMAGE3D ? 5 : 1;
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

    auto perChannelDataSize = 0;
    switch (imageFormat.image_channel_data_type) {
    case CL_UNORM_INT8:
        perChannelDataSize = 1;
        break;
    case CL_SIGNED_INT16:
    case CL_HALF_FLOAT:
        perChannelDataSize = 2;
        break;
    case CL_UNSIGNED_INT32:
    case CL_FLOAT:
        perChannelDataSize = 4;
        break;
    }

    auto numChannels = 0u;
    switch (imageFormat.image_channel_order) {
    case CL_R:
        numChannels = 1;
        break;
    case CL_RG:
        numChannels = 2;
        break;
    case CL_RGBA:
        numChannels = 4;
        break;
    }
    size_t elementSize = perChannelDataSize * numChannels;

    // Generate initial src memory but make it unaligned to
    // stress test enqueueWriteImage logic
    auto srcMemoryAligned = alignedMalloc(1 + elementSize * numPixels, 0x1000);
    auto srcMemoryUnaligned =
        ptrOffset(reinterpret_cast<uint8_t *>(srcMemoryAligned), 1);

    for (unsigned i = 0; i < numPixels * elementSize; ++i) {
        uint8_t origValue = i;
        memcpy(srcMemoryUnaligned + i, &origValue, sizeof(origValue));
    }

    // Initialize dest memory
    auto sizeMemory = testWidth * testHeight * testDepth * elementSize;
    auto dstMemory = new (std::nothrow) uint8_t[sizeMemory];
    ASSERT_NE(nullptr, dstMemory);
    memset(dstMemory, 0xff, sizeMemory);

    auto retVal = CL_INVALID_VALUE;
    cl_mem_flags flags = 0;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    dstImage = Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, dstImage);
    memset(dstImage->getCpuAddress(), 0xFF, dstImage->getSize()); // init image - avoid writeImage inside createImage (for tiled img)

    auto origin = std::get<2>(GetParam()).offsets;

    // Only draw 1/4 of the original image
    const size_t region[3] = {std::max(testWidth / 2, 1u),
                              std::max(testHeight / 2, 1u),
                              std::max(testDepth / 2, 1u)};

    // Offset the source memory
    auto pSrcMemory = ptrOffset(srcMemoryUnaligned, (origin[1] * testWidth + origin[0]) * elementSize);
    size_t inputRowPitch = testWidth * elementSize;
    size_t inputSlicePitch = inputRowPitch * testHeight;
    retVal = pCmdQ->enqueueWriteImage(
        dstImage,
        CL_TRUE,
        origin,
        region,
        inputRowPitch,
        inputSlicePitch,
        pSrcMemory,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto readMemory = new uint8_t[dstImage->getSize()];
    size_t imgOrigin[] = {0, 0, 0};
    size_t imgRegion[] = {testWidth, testHeight, testDepth};
    pCmdQ->enqueueReadImage(dstImage, CL_TRUE, imgOrigin, imgRegion, 0, 0, readMemory, nullptr, 0, nullptr, nullptr);

    auto pDstMemory = readMemory;
    auto pSrc = pSrcMemory;

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
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&pDstMemory[pos], pSrc, elementSize);
                    pSrc = ptrOffset(pSrc, elementSize);
                } else {
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&pDstMemory[pos], dstMemory, elementSize);
                }
            }
            pDstMemory = ptrOffset(pDstMemory, rowPitch);
            if (y >= origin[1] && y < origin[1] + region[1] &&
                z >= origin[2] && z < origin[2] + region[2]) {
                pSrc = ptrOffset(pSrc, inputRowPitch - (elementSize * region[0]));
            }
        }
        pDstMemory = ptrOffset(pDstMemory, slicePitch - (rowPitch * (testHeight > 0 ? testHeight : 1)));
        if (z >= origin[2] && z < origin[2] + region[2]) {
            pSrc = ptrOffset(pSrc, inputSlicePitch - (inputRowPitch * (region[1])));
        }
    }

    alignedFree(srcMemoryAligned);
    delete[] dstMemory;
    delete[] readMemory;
}

INSTANTIATE_TEST_CASE_P(AUBWriteImage_simple, AUBWriteImage,
                        ::testing::Combine(::testing::Values( // formats
                                               CL_UNORM_INT8, CL_SIGNED_INT16,
                                               CL_UNSIGNED_INT32, CL_HALF_FLOAT,
                                               CL_FLOAT),
                                           ::testing::Values( // channels
                                               CL_R, CL_RG, CL_RGBA),
                                           ::testing::ValuesIn(writeImageParams)));

using AUBWriteImageUnaligned = AUBImageUnaligned;

HWTEST_F(AUBWriteImageUnaligned, misalignedHostPtr) {
    const std::vector<size_t> pixelSizes = {1, 2, 4};
    const std::vector<size_t> offsets = {0, 1, 2, 3};
    const std::vector<size_t> sizes = {3, 2, 1};

    for (auto pixelSize : pixelSizes) {
        for (auto offset : offsets) {
            for (auto size : sizes) {
                testWriteImageUnaligned<FamilyType>(offset, size, pixelSize);
            }
        }
    }
}
