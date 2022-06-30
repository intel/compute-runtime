/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

static const unsigned int testImageDimensions = 32;

class CreateImage1DTest : public ClDeviceFixture,
                          public testing::TestWithParam<uint32_t /*cl_mem_object_type*/> {
  public:
    CreateImage1DTest() {
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
        imageDesc.image_height = 0;
        imageDesc.image_depth = 0;
        imageDesc.image_array_size = 1;
        imageDesc.image_row_pitch = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels = 0;
        imageDesc.num_samples = 0;
        imageDesc.mem_object = NULL;
        // clang-format on

        if (types == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
            imageDesc.image_array_size = 10;
        }
        context = new MockContext(pClDevice);

        if (types == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
            imageDesc.mem_object = clCreateBuffer(context, CL_MEM_ALLOC_HOST_PTR, testImageDimensions, nullptr, nullptr);
        }
    }

    void TearDown() override {
        if (types == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
            clReleaseMemObject(imageDesc.mem_object);
        }
        delete context;
        ClDeviceFixture::TearDown();
    }

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    MockContext *context;
    cl_mem_object_type types = 0;
};

typedef CreateImage1DTest CreateImage1DType;

HWTEST_P(CreateImage1DType, GivenValidTypeWhenCreatingImageThenImageParamsAreCorrect) {
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, pClDevice->getHardwareInfo().capabilityTable.supportsOcl21Features);
    auto image = Image::create(
        context,
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, pDevice),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        nullptr,
        retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    auto imgDesc = image->getImageDesc();

    EXPECT_NE(0u, imgDesc.image_width);
    EXPECT_EQ(0u, imgDesc.image_height);
    EXPECT_EQ(0u, imgDesc.image_depth);
    EXPECT_NE(0u, imgDesc.image_row_pitch);
    EXPECT_GE(imgDesc.image_slice_pitch, imgDesc.image_row_pitch);

    size_t imageInfoHeight = 0;
    retVal = clGetImageInfo(image, CL_IMAGE_HEIGHT, sizeof(size_t), &imageInfoHeight, NULL);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(0u, imageInfoHeight);

    if ((types == CL_MEM_OBJECT_IMAGE1D) || (types == CL_MEM_OBJECT_IMAGE1D_BUFFER)) {
        EXPECT_EQ(0u, imgDesc.image_array_size);
    } else if (types == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
        EXPECT_NE(0u, imgDesc.image_array_size);
    } else {
        ASSERT_TRUE(false);
    }

    EXPECT_EQ(image->getCubeFaceIndex(), static_cast<uint32_t>(__GMM_NO_CUBE_MAP));

    ASSERT_EQ(true, image->isMemObjZeroCopy());
    EXPECT_FALSE(image->isImageFromImage());

    auto address = image->getCpuAddress();
    EXPECT_NE(nullptr, address);

    if (types == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
        Buffer *inputBuffer = castToObject<Buffer>(imageDesc.buffer);
        EXPECT_NE(nullptr, inputBuffer->getCpuAddress());
        EXPECT_EQ(inputBuffer->getCpuAddress(), image->getCpuAddress());
        EXPECT_FALSE(image->getIsObjectRedescribed());
        EXPECT_GE(2, inputBuffer->getRefInternalCount());
        EXPECT_TRUE(image->isImageFromBuffer());
    } else {
        EXPECT_FALSE(image->isImageFromBuffer());
    }

    typedef typename FamilyType::RENDER_SURFACE_STATE SURFACE_STATE;
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image);
    EXPECT_EQ(SURFACE_STATE::SURFACE_TYPE_SURFTYPE_1D, imageHw->surfaceType);
    delete image;
}

static cl_mem_object_type Image1DTypes[] = {
    CL_MEM_OBJECT_IMAGE1D,
    CL_MEM_OBJECT_IMAGE1D_BUFFER,
    CL_MEM_OBJECT_IMAGE1D_ARRAY};

INSTANTIATE_TEST_CASE_P(
    CreateImage1DTestCreate,
    CreateImage1DType,
    testing::ValuesIn(Image1DTypes));
