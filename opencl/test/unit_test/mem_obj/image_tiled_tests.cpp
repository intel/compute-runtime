/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

static const auto dimension = 16;
static auto channelType = CL_UNORM_INT8;
static auto channelOrder = CL_RGBA;

class CreateTiledImageTest : public ClDeviceFixture,
                             public testing::TestWithParam<uint32_t>,
                             public CommandQueueHwFixture {
    typedef CommandQueueHwFixture CommandQueueFixture;

  public:
    CreateTiledImageTest() {
    }

  protected:
    void SetUp() override {
        ClDeviceFixture::setUp();
        CommandQueueFixture::setUp(pClDevice, 0);
        type = GetParam();

        // clang-format off
        imageFormat.image_channel_data_type = channelType;
        imageFormat.image_channel_order     = channelOrder;

        imageDesc.image_type        = type;
        imageDesc.image_width       = dimension;
        imageDesc.image_height      = dimension;
        imageDesc.image_depth       = 1;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object = NULL;
        // clang-format on
    }

    void TearDown() override {
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    cl_mem_object_type type = 0;
};

HWTEST_P(CreateTiledImageTest, GivenImageTypeWhenCheckingIsTiledThenTrueReturnedForTiledImage) {
    MockContext context;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, pClDevice->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        &context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, pDevice),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(defaultHwInfo->capabilityTable.supportsImages, image->isTiledAllocation());

    delete image;
}

TEST_P(CreateTiledImageTest, GivenSharedTiledImageWhenCheckingIsTiledThenTrueReturned) {
    MockContext context;
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation(0, 0x1000);
    ImageInfo info = {};
    McsSurfaceInfo msi = {};
    ClSurfaceFormatInfo surfaceFormat;
    surfaceFormat.surfaceFormat.gmmSurfaceFormat = GMM_FORMAT_B8G8R8A8_UNORM;
    info.surfaceFormat = &surfaceFormat.surfaceFormat;

    info.imgDesc = Image::convertDescriptor(imageDesc);
    info.plane = GMM_NO_PLANE;

    auto gmm = MockGmm::queryImgParams(context.getDevice(0)->getGmmHelper(), info, false);

    alloc->setDefaultGmm(gmm.release());

    auto image = Image::createSharedImage(
        &context,
        nullptr,
        msi,
        GraphicsAllocationHelper::toMultiGraphicsAllocation(alloc),
        nullptr,
        CL_MEM_READ_WRITE,
        0,
        &surfaceFormat,
        info,
        0, 0, 0, false);

    ASSERT_NE(nullptr, image);

    EXPECT_TRUE(image->isTiledAllocation());

    delete image;
}

typedef CreateTiledImageTest CreateNonTiledImageTest;

TEST_P(CreateNonTiledImageTest, GivenSharedNonTiledImageWhenCheckingIsTiledThenFalseReturned) {
    MockContext context;
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation(0, 0x1000);
    ImageInfo info = {};
    McsSurfaceInfo msi = {};
    ClSurfaceFormatInfo surfaceFormat;

    imageDesc.image_height = 1;

    surfaceFormat.surfaceFormat.gmmSurfaceFormat = GMM_FORMAT_B8G8R8A8_UNORM;
    info.surfaceFormat = &surfaceFormat.surfaceFormat;

    info.imgDesc = Image::convertDescriptor(imageDesc);
    info.plane = GMM_NO_PLANE;
    info.linearStorage = true;

    auto gmm = MockGmm::queryImgParams(context.getDevice(0)->getGmmHelper(), info, false);

    alloc->setDefaultGmm(gmm.release());

    auto image = Image::createSharedImage(
        &context,
        nullptr,
        msi,
        GraphicsAllocationHelper::toMultiGraphicsAllocation(alloc),
        nullptr,
        CL_MEM_READ_WRITE,
        0,
        &surfaceFormat,
        info,
        0, 0, 0, false);

    ASSERT_NE(nullptr, image);

    EXPECT_FALSE(image->isTiledAllocation());
    EXPECT_EQ(info.linearStorage, image->getGraphicsAllocation(context.getDevice(0)->getRootDeviceIndex())->getDefaultGmm()->gmmResourceInfo->getResourceFlags()->Info.Linear);

    delete image;
}

// Parameterized test that tests image creation with tiled types
static uint32_t tiledImageTypes[] = {
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY,
    CL_MEM_OBJECT_IMAGE3D};

static uint32_t nonTiledImageTypes[] = {
    CL_MEM_OBJECT_IMAGE1D};

INSTANTIATE_TEST_SUITE_P(CreateTiledImageTest, CreateTiledImageTest, testing::ValuesIn(tiledImageTypes));
INSTANTIATE_TEST_SUITE_P(CreateNonTiledImageTest, CreateNonTiledImageTest, testing::ValuesIn(nonTiledImageTypes));
