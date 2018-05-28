/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

struct MapImageParams {
    cl_mem_object_type imageType;
    size_t offsets[3];
} mapImageParams[] = {
    {CL_MEM_OBJECT_IMAGE1D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE1D, {1u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE2D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE2D, {1u, 2u, 0u}},
    {CL_MEM_OBJECT_IMAGE3D, {0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE3D, {1u, 2u, 3u}},
};

struct AUBMapImage
    : public CommandDeviceFixture,
      public AUBCommandStreamFixture,
      public ::testing::WithParamInterface<std::tuple<uint32_t /*cl_channel_type*/, uint32_t /*cl_channel_order*/, MapImageParams>>,
      public ::testing::Test {
    typedef AUBCommandStreamFixture CommandStreamFixture;

    using AUBCommandStreamFixture::SetUp;

    AUBMapImage() {
    }

    void SetUp() override {
        CommandDeviceFixture::SetUp(cl_command_queue_properties(0));
        CommandStreamFixture::SetUp(pCmdQ);
        context = new MockContext(pDevice);
    }

    void TearDown() override {
        delete srcImage;
        delete context;
        CommandStreamFixture::TearDown();
        CommandDeviceFixture::TearDown();
    }

    MockContext *context;
    Image *srcImage = nullptr;
};

HWTEST_P(AUBMapImage, MapUpdateUnmapVerify) {
    const unsigned int testWidth = 5;
    const unsigned int testHeight =
        std::get<2>(GetParam()).imageType != CL_MEM_OBJECT_IMAGE1D ? 5 : 1;
    const unsigned int testDepth =
        std::get<2>(GetParam()).imageType == CL_MEM_OBJECT_IMAGE3D ? 5 : 1;

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    // clang-format off
    imageFormat.image_channel_data_type = std::get<0>( GetParam( ) );
    imageFormat.image_channel_order = std::get<1>( GetParam( ) );

    imageDesc.image_type = std::get<2>( GetParam( ) ).imageType;
    imageDesc.image_width = testWidth;
    imageDesc.image_height = testHeight;
    imageDesc.image_depth = testDepth;
    imageDesc.image_array_size = 1;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;
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
    auto sizeMemory = testWidth * alignUp(testHeight, 4) * testDepth * elementSize;
    auto srcMemory = new (std::nothrow) uint8_t[sizeMemory];
    ASSERT_NE(nullptr, srcMemory);

    for (unsigned i = 0; i < sizeMemory; ++i) {
        uint8_t origValue = i;
        memcpy(srcMemory + i, &origValue, sizeof(origValue));
    }

    auto retVal = CL_INVALID_VALUE;
    cl_mem_flags flags = CL_MEM_COPY_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    srcImage = Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        srcMemory,
        retVal);
    ASSERT_NE(nullptr, srcImage);

    auto origin = std::get<2>(GetParam()).offsets;

    const size_t region[3] = {std::max(testWidth / 2, 1u),
                              std::max(testHeight / 2, 1u),
                              std::max(testDepth / 2, 1u)};

    size_t inputRowPitch = testWidth * elementSize;
    size_t inputSlicePitch = inputRowPitch * testHeight;

    size_t mappedRowPitch;
    size_t mappedSlicePitch;
    auto mappedPtr = pCmdQ->enqueueMapImage(srcImage, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ,
                                            origin, region, &mappedRowPitch, &mappedSlicePitch,
                                            0, nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    uint8_t *mappedPtrStart;
    uint8_t *srcMemoryStart;

    if (srcImage->allowTiling()) {
        mappedPtrStart = static_cast<uint8_t *>(mappedPtr);
        srcMemoryStart = srcMemory;

        // validate mapped region
        srcMemoryStart = ptrOffset(srcMemoryStart, inputSlicePitch * origin[2]);
        srcMemoryStart = ptrOffset(srcMemoryStart, inputRowPitch * origin[1]);
        srcMemoryStart = ptrOffset(srcMemoryStart, elementSize * origin[0]);
        for (size_t z = 0; z < region[2]; z++) {
            for (size_t y = 0; y < region[1]; y++) {
                AUBCommandStreamFixture::expectMemory<FamilyType>(mappedPtrStart, srcMemoryStart, elementSize * region[0]);
                mappedPtrStart = ptrOffset(mappedPtrStart, mappedRowPitch);
                srcMemoryStart = ptrOffset(srcMemoryStart, inputRowPitch);
            }
            mappedPtrStart = ptrOffset(mappedPtrStart, mappedSlicePitch - (mappedRowPitch * region[1]));
            srcMemoryStart = ptrOffset(srcMemoryStart, inputSlicePitch - (inputRowPitch * (region[1])));
        }
    }

    // write to mapped ptr
    mappedPtrStart = static_cast<uint8_t *>(mappedPtr);
    for (size_t z = 0; z < region[2]; z++) {
        for (size_t y = 0; y < region[1]; y++) {
            memset(mappedPtrStart, 0xFF, elementSize * region[0]);
            mappedPtrStart = ptrOffset(mappedPtrStart, mappedRowPitch);
        }
        mappedPtrStart = ptrOffset(mappedPtrStart, mappedSlicePitch - (mappedRowPitch * region[1]));
    }
    pCmdQ->enqueueUnmapMemObject(srcImage, mappedPtr, 0, nullptr, nullptr);

    // verify unmap
    uint8_t *readMemory = new uint8_t[srcImage->getSize()];
    size_t imgOrigin[] = {0, 0, 0};
    size_t imgRegion[] = {testWidth, testHeight, testDepth};
    retVal = pCmdQ->enqueueReadImage(srcImage, CL_TRUE, imgOrigin, imgRegion, inputRowPitch, inputSlicePitch, readMemory, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    srcMemoryStart = srcMemory;
    auto readMemoryStart = readMemory;
    uint8_t *expected = new uint8_t[elementSize];
    memset(expected, 0xFF, elementSize);

    for (size_t z = 0; z < testDepth; ++z) {
        for (size_t y = 0; y < testHeight; ++y) {
            for (size_t x = 0; x < testWidth; ++x) {
                if (z >= origin[2] && z < (origin[2] + region[2]) &&
                    y >= origin[1] && y < (origin[1] + region[1]) &&
                    x >= origin[0] && x < (origin[0] + region[0])) {
                    // this texel should be updated
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&readMemoryStart[x * elementSize], expected, elementSize);
                } else {
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&readMemoryStart[x * elementSize], &srcMemoryStart[x * elementSize], elementSize);
                }
            }
            readMemoryStart = ptrOffset(readMemoryStart, inputRowPitch);
            srcMemoryStart = ptrOffset(srcMemoryStart, inputRowPitch);
        }
        readMemoryStart = ptrOffset(readMemoryStart, inputSlicePitch - (inputRowPitch * (testHeight > 0 ? testHeight : 1)));
        srcMemoryStart = ptrOffset(srcMemoryStart, inputSlicePitch - (inputRowPitch * (testHeight > 0 ? testHeight : 1)));
    }

    delete[] readMemory;
    delete[] srcMemory;
    delete[] expected;
}

INSTANTIATE_TEST_CASE_P(
    AUBMapImage_simple, AUBMapImage,
    ::testing::Combine(::testing::Values( // formats
                           CL_UNORM_INT8, CL_SIGNED_INT16, CL_UNSIGNED_INT32,
                           CL_HALF_FLOAT, CL_FLOAT),
                       ::testing::Values( // channels
                           CL_R, CL_RG, CL_RGBA),
                       ::testing::ValuesIn(mapImageParams)));
