/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include <algorithm>

using namespace NEO;

// clang-format off
struct FillImageParams {
    cl_mem_object_type imageType;
    size_t offsets[3];
} imageParams[] = {
    {CL_MEM_OBJECT_IMAGE1D, { 0u, 0u, 0u}}, 
    {CL_MEM_OBJECT_IMAGE1D, { 1u, 0u, 0u}}, 
    {CL_MEM_OBJECT_IMAGE2D, { 0u, 0u, 0u}}, 
    {CL_MEM_OBJECT_IMAGE2D, { 1u, 2u, 0u}},
    {CL_MEM_OBJECT_IMAGE3D, { 0u, 0u, 0u}},
    {CL_MEM_OBJECT_IMAGE3D, { 1u, 2u, 3u}}
};

static const uint32_t fillValues[4] = { 0x3f800000, 0x00000000, 0x3f555555, 0x3f2aaaaa };

static const uint16_t expectedHALF_FLOAT[4] = {0x3c00, 0x0000, 0x3aaa, 0x3955};

static const uint16_t expectedUNORM16[4]    = {0xffff, 0x0000, 0xd554, 0xaaa9};
static const uint8_t  expectedUNORM8[4]     = {  0xff,   0x00,   0xd4,   0xa9};
//The distance between sRGB values and the expected values should not be greater than 0.6f
//In this test, for simplicity purposes, we are checking if the distance is 0
static const uint8_t  expectedUNORM8sRGB[4]     = {  0xff,   0x00,   0xeb,   0xa9};
static const uint8_t  expectedUNORM8sBGR[4]     = {  0xeb,   0x00,   0xff,   0xa9};

static const uint16_t expectedSNORM16[4]    = {0x7fff, 0x0000, 0x6AA9, 0x5554};
static const uint8_t  expectedSNORM8[4]     = {  0x7f,   0x00,   0x69,   0x54};

static auto     expectedSINT32    = fillValues;
static uint16_t expectedSINT16[4] = { 0x0000, 0x0000, 0x5555, 0xaaaa };
static uint8_t  expectedSINT8[4]  = {   0x00,   0x00,   0x55,   0xaa };

static auto     expectedUINT32    = fillValues;
static uint16_t expectedUINT16[4] = { 0x0000, 0x0000, 0x5555, 0xaaaa };
static uint8_t  expectedUINT8[4]  = {   0x00,   0x00,   0x55,   0xaa };

static auto expectedFLOAT = fillValues;

// ChannelTypes/FillValues for test
struct FillChannelType {
    cl_channel_type type;
    const void *expectedValues;
} fillChannelTypes[] = {
    {CL_SNORM_INT8,     expectedSNORM8},
    {CL_SNORM_INT16,    expectedSNORM16},
    {CL_UNORM_INT8,     expectedUNORM8},
    {CL_UNORM_INT16,    expectedUNORM16},
    {CL_SIGNED_INT8,    expectedSINT8},
    {CL_SIGNED_INT16,   expectedSINT16},
    {CL_SIGNED_INT32,   expectedSINT32},
    {CL_UNSIGNED_INT8,  expectedUINT8},
    {CL_UNSIGNED_INT16, expectedUINT16},
    {CL_UNSIGNED_INT32, expectedUINT32},
    {CL_HALF_FLOAT,     expectedHALF_FLOAT},
    {CL_FLOAT,          expectedFLOAT}};
// clang-format on

struct AubFillImage
    : public CommandDeviceFixture,
      public AUBCommandStreamFixture,
      public ::testing::WithParamInterface<std::tuple<FillChannelType, uint32_t /*cl_channel_order*/, FillImageParams>>,
      public ::testing::Test {

    using AUBCommandStreamFixture::SetUp;

    typedef AUBCommandStreamFixture CommandStreamFixture;

    void SetUp() override {
        CommandDeviceFixture::SetUp(cl_command_queue_properties(0));
        CommandStreamFixture::SetUp(pCmdQ);
        context = new MockContext(pDevice);
    }

    void TearDown() override {
        delete image;
        delete context;
        CommandStreamFixture::TearDown();
        CommandDeviceFixture::TearDown();
    }

    MockContext *context;
    Image *image = nullptr;
};

HWTEST_P(AubFillImage, simple) {
    const unsigned int testWidth = 5;
    const unsigned int testHeight = std::get<2>(GetParam()).imageType != CL_MEM_OBJECT_IMAGE1D ? 5 : 1;
    const unsigned int testDepth = std::get<2>(GetParam()).imageType == CL_MEM_OBJECT_IMAGE3D ? 5 : 1;

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    // clang-format off
    imageFormat.image_channel_data_type = std::get<0>(GetParam()).type;
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
    default:
    case CL_SIGNED_INT8:
    case CL_SNORM_INT8:
    case CL_UNORM_INT8:
    case CL_UNSIGNED_INT8:
        perChannelDataSize = 1;
        break;
    case CL_HALF_FLOAT:
    case CL_SIGNED_INT16:
    case CL_SNORM_INT16:
    case CL_UNORM_INT16:
    case CL_UNSIGNED_INT16:
        perChannelDataSize = 2;
        break;
    case CL_SIGNED_INT32:
    case CL_UNSIGNED_INT32:
    case CL_FLOAT:
        perChannelDataSize = 4;
        break;
    }

    auto numChannels = 0u;
    switch (imageFormat.image_channel_order) {
    default:
    case CL_R:
        numChannels = 1;
        break;
    case CL_RG:
        numChannels = 2;
        break;
    case CL_RGBA:
    case CL_sRGBA:
    case CL_sBGRA:
        numChannels = 4;
        break;
    }
    size_t elementSize = perChannelDataSize * numChannels;

    auto retVal = CL_INVALID_VALUE;
    if (imageFormat.image_channel_data_type != CL_UNORM_INT8 && (imageFormat.image_channel_order == CL_sRGBA || imageFormat.image_channel_order == CL_sBGRA))
        return; //sRGBA and sBGRA support only unorm int8 type, so other cases will return from the test with a success
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    image = Image::create(
        context,
        flags,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);

    ASSERT_NE(nullptr, image);

    auto sizeMemory = image->getSize();
    ASSERT_GT(sizeMemory, 0u);

    auto srcMemory = new uint8_t[elementSize];
    memset(srcMemory, 0xAB, elementSize);
    memset(image->getCpuAddress(), 0xAB, sizeMemory);

    auto origin = std::get<2>(GetParam()).offsets;

    const size_t region[3] = {
        std::max(testWidth / 2, 1u),
        std::max(testHeight / 2, 1u),
        std::max(testDepth / 2, 1u)};

    retVal = pCmdQ->enqueueFillImage(
        image,
        fillValues,
        origin,
        region,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t imgOrigin[] = {0, 0, 0};
    size_t imgRegion[] = {testWidth, testHeight, testDepth};

    auto dstMemory = new uint8_t[sizeMemory];
    pCmdQ->enqueueReadImage(image, CL_TRUE, imgOrigin, imgRegion, 0, 0, dstMemory, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t slicePitch = image->getHostPtrSlicePitch();
    size_t rowPitch = image->getHostPtrRowPitch();

    auto expected = std::get<0>(GetParam()).expectedValues;
    if (imageFormat.image_channel_order == CL_sRGBA) {
        expected = expectedUNORM8sRGB;
    }
    if (imageFormat.image_channel_order == CL_sBGRA) {
        expected = expectedUNORM8sBGR;
    }

    auto pImageData = dstMemory;

    for (size_t z = 0; z < testDepth; ++z) {
        for (size_t y = 0; y < testHeight; ++y) {
            for (size_t x = 0; x < testWidth; ++x) {
                if (z >= origin[2] && z < (origin[2] + region[2]) &&
                    y >= origin[1] && y < (origin[1] + region[1]) &&
                    x >= origin[0] && x < (origin[0] + region[0])) {
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&pImageData[x * elementSize], expected, elementSize);
                } else {
                    AUBCommandStreamFixture::expectMemory<FamilyType>(&pImageData[x * elementSize], srcMemory, elementSize);
                }
            }
            pImageData = ptrOffset(pImageData, rowPitch);
        }
        pImageData = ptrOffset(pImageData, slicePitch - (rowPitch * (testHeight > 0 ? testHeight : 1)));
    }

    delete[] dstMemory;
    delete[] srcMemory;
}

INSTANTIATE_TEST_CASE_P(AubFillImage_simple,
                        AubFillImage,
                        ::testing::Combine(
                            ::testing::ValuesIn(fillChannelTypes),
                            ::testing::Values( // channels
                                CL_R,
                                CL_RG,
                                CL_RGBA,
                                CL_sRGBA,
                                CL_sBGRA),
                            ::testing::ValuesIn(imageParams)));
