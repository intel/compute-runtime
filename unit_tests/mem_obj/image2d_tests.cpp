/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/mem_obj/image.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/helpers/unit_test_helper.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

static const unsigned int testImageDimensions = 32;

class CreateImage2DTest : public DeviceFixture,
                          public testing::TestWithParam<uint32_t /*cl_mem_object_type*/> {
  public:
    CreateImage2DTest() {
    }

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
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
        context = new MockContext(pDevice);
    }

    void TearDown() override {
        delete context;
        DeviceFixture::TearDown();
    }
    Image *createImageWithFlags(cl_mem_flags flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        return Image::create(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal);
    }
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    cl_mem_object_type types = 0;
};

typedef CreateImage2DTest CreateImage2DType;

HWTEST_P(CreateImage2DType, validTypes) {
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
    CreateImage2DTest_Create,
    CreateImage2DType,
    testing::ValuesIn(Image2DTypes));
