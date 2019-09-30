/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/helpers/ptr_math.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/aub_tests/command_queue/enqueue_read_write_image_aub_fixture.h"
#include "unit_tests/mocks/mock_context.h"

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

struct AUBReadImage
    : public CommandDeviceFixture,
      public AUBCommandStreamFixture,
      public ::testing::WithParamInterface<std::tuple<uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, ReadImageParams>>,
      public ::testing::Test {

    typedef AUBCommandStreamFixture CommandStreamFixture;

    using AUBCommandStreamFixture::SetUp;

    void SetUp() override {
        CommandDeviceFixture::SetUp(cl_command_queue_properties(0));
        CommandStreamFixture::SetUp(pCmdQ);
        if (!pDevice->getDeviceInfo().imageSupport) {
            GTEST_SKIP();
        }
        context = std::make_unique<MockContext>(pDevice);
    }

    void TearDown() override {
        srcImage.reset();
        context.reset();

        CommandStreamFixture::TearDown();
        CommandDeviceFixture::TearDown();
    }

    std::unique_ptr<MockContext> context;
    std::unique_ptr<Image> srcImage;
};

HWTEST_P(AUBReadImage, simpleUnalignedMemory) {

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

    // Generate initial dst memory but make it unaligned to
    // stress test enqueueReadImage logic
    auto dstMemoryAligned = alignedMalloc(1 + elementSize * numPixels, 0x1000);
    auto dstMemoryUnaligned =
        ptrOffset(reinterpret_cast<uint8_t *>(dstMemoryAligned), 1);

    auto sizeMemory = testWidth * alignUp(testHeight, 4) * testDepth * elementSize;
    auto srcMemory = new (std::nothrow) uint8_t[sizeMemory];
    ASSERT_NE(nullptr, srcMemory);

    for (unsigned i = 0; i < sizeMemory; ++i) {
        uint8_t origValue = i;
        memcpy(srcMemory + i, &origValue, sizeof(origValue));
    }

    for (unsigned i = 0; i < numPixels * elementSize; ++i) {
        uint8_t origValue = 0xff;
        memcpy(dstMemoryUnaligned + i, &origValue, sizeof(origValue));
    }

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto retVal = CL_INVALID_VALUE;
    srcImage.reset(Image::create(
        context.get(),
        flags,
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

    size_t inputRowPitch = testWidth * elementSize;
    size_t inputSlicePitch = inputRowPitch * testHeight;

    retVal = pCmdQ->enqueueReadImage(
        srcImage.get(),
        CL_FALSE,
        origin,
        region,
        inputRowPitch,
        inputSlicePitch,
        dstMemoryUnaligned,
        nullptr,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pCmdQ->flush();
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto imageMemory = srcMemory;

    bool isGpuCopy = srcImage->isTiledAllocation() || !MemoryPool::isSystemMemoryPool(srcImage->getGraphicsAllocation()->getMemoryPool());
    if (!isGpuCopy) {
        imageMemory = (uint8_t *)(srcImage->getCpuAddress());
    }

    auto pSrcMemory =
        ptrOffset(imageMemory, (origin[2] * testWidth * testHeight + origin[1] * testWidth + origin[0]) * elementSize);

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

        pDstMemory =
            ptrOffset(dstMemoryUnaligned, testWidth * testHeight * elementSize);
    }

    retVal = pCmdQ->finish(); //FixMe - not all test cases verified with expects
    EXPECT_EQ(CL_SUCCESS, retVal);

    alignedFree(dstMemoryAligned);
    delete[] srcMemory;
}

INSTANTIATE_TEST_CASE_P(
    AUBReadImage_simple, AUBReadImage,
    ::testing::Combine(::testing::Values( // formats
                           CL_UNORM_INT8, CL_SIGNED_INT16, CL_UNSIGNED_INT32,
                           CL_HALF_FLOAT, CL_FLOAT),
                       ::testing::Values( // channels
                           CL_R, CL_RG, CL_RGBA),
                       ::testing::ValuesIn(readImageParams)));

using AUBReadImageUnaligned = AUBImageUnaligned;

HWTEST_F(AUBReadImageUnaligned, misalignedHostPtr) {
    const std::vector<size_t> pixelSizes = {1, 2, 4};
    const std::vector<size_t> offsets = {0, 1, 2, 3};
    const std::vector<size_t> sizes = {3, 2, 1};

    for (auto pixelSize : pixelSizes) {
        for (auto offset : offsets) {
            for (auto size : sizes) {
                testReadImageUnaligned<FamilyType>(offset, size, pixelSize);
            }
        }
    }
}
