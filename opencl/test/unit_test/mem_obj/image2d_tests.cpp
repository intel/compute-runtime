/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

static const unsigned int testImageDimensions = 32;

class CreateImage2DTest : public ClDeviceFixture,
                          public testing::TestWithParam<uint32_t /*cl_mem_object_type*/> {
  public:
    CreateImage2DTest() {
    }

  protected:
    void SetUp() override {
        ClDeviceFixture::SetUp();
        types = GetParam();

        // clang-format off
        imageFormat.image_channel_data_type = CL_UNORM_INT8;
        imageFormat.image_channel_order = CL_RGBA;

        imageDesc.image_type = types;
        imageDesc.image_width = testImageDimensions;
        imageDesc.image_height = testImageDimensions;
        imageDesc.image_depth = 0;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;
        // clang-format on

        if (types == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
            imageDesc.image_array_size = 10;
        }
        context = new MockContext(pClDevice);
    }

    void TearDown() override {
        delete context;
        ClDeviceFixture::TearDown();
    }
    Image *createImageWithFlags(cl_mem_flags flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(
            flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        return Image::create(context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
                             flags, 0, surfaceFormat, &imageDesc, nullptr, retVal);
    }
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    cl_mem_object_type types = 0;
};

typedef CreateImage2DTest CreateImage2DType;

HWTEST_P(CreateImage2DType, GivenValidTypeWhenCreatingImageThenImageCreatedWithCorrectParams) {
    auto image = createImageWithFlags(CL_MEM_READ_WRITE);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    auto imgDesc = image->getImageDesc();

    EXPECT_NE(0u, imgDesc.image_width);
    EXPECT_NE(0u, imgDesc.image_height);
    EXPECT_EQ(0u, imgDesc.image_depth);
    EXPECT_NE(0u, imgDesc.image_row_pitch);
    EXPECT_GE(imgDesc.image_slice_pitch, imgDesc.image_row_pitch);

    if (types == CL_MEM_OBJECT_IMAGE2D) {
        EXPECT_EQ(0u, imgDesc.image_array_size);
    } else if (types == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        EXPECT_NE(0u, imgDesc.image_array_size);
    } else {
        ASSERT_TRUE(false);
    }

    EXPECT_EQ(image->getCubeFaceIndex(), static_cast<uint32_t>(__GMM_NO_CUBE_MAP));
    EXPECT_EQ(!UnitTestHelper<FamilyType>::tiledImagesSupported, image->isMemObjZeroCopy());

    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image);
    EXPECT_EQ(SURFACE_STATE::SURFACE_TYPE_SURFTYPE_2D, imageHw->surfaceType);

    SurfaceOffsets surfaceOffsets;
    image->getSurfaceOffsets(surfaceOffsets);

    EXPECT_EQ(0u, surfaceOffsets.offset);
    EXPECT_EQ(0u, surfaceOffsets.xOffset);
    EXPECT_EQ(0u, surfaceOffsets.yOffset);
    EXPECT_EQ(0u, surfaceOffsets.yOffsetForUVplane);

    delete image;
}

static cl_mem_object_type Image2DTypes[] = {
    CL_MEM_OBJECT_IMAGE2D,
    CL_MEM_OBJECT_IMAGE2D_ARRAY};

INSTANTIATE_TEST_CASE_P(
    CreateImage2DTestCreate,
    CreateImage2DType,
    testing::ValuesIn(Image2DTypes));
