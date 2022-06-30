/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"
#include "shared/test/common/mocks/mock_kernel_info.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/kernel/image_transformer.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

using namespace NEO;

class ImageTransformerTest : public ::testing::Test {
  public:
    void SetUp() override {
        using SimpleKernelArgInfo = Kernel::SimpleKernelArgInfo;
        pKernelInfo = std::make_unique<MockKernelInfo>();

        pKernelInfo->addArgImage(0, firstImageOffset, iOpenCL::IMAGE_MEMORY_OBJECT_2D, true);
        pKernelInfo->addArgImage(1, secondImageOffset, iOpenCL::IMAGE_MEMORY_OBJECT_2D, true);

        image1.reset(Image3dHelper<>::create(&context));
        image2.reset(Image3dHelper<>::create(&context));
        SimpleKernelArgInfo imageArg1;
        SimpleKernelArgInfo imageArg2;
        clImage1 = static_cast<cl_mem>(image2.get());
        clImage2 = static_cast<cl_mem>(image2.get());
        imageArg1.value = &clImage1;
        imageArg1.object = clImage1;
        imageArg2.value = &clImage2;
        imageArg2.object = clImage2;
        kernelArguments.push_back(imageArg1);
        kernelArguments.push_back(imageArg2);
    }
    const int firstImageOffset = 0x20;
    const int secondImageOffset = 0x40;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    ImageTransformer imageTransformer;
    MockContext context;
    std::unique_ptr<Image> image1;
    std::unique_ptr<Image> image2;
    cl_mem clImage1;
    cl_mem clImage2;
    char ssh[0x80];
    std::vector<Kernel::SimpleKernelArgInfo> kernelArguments;
};

TEST_F(ImageTransformerTest, givenImageTransformerWhenRegisterImage3dThenTransformerHasRegisteredImages3d) {
    bool retVal;
    retVal = imageTransformer.hasRegisteredImages3d();
    EXPECT_FALSE(retVal);
    imageTransformer.registerImage3d(0);
    retVal = imageTransformer.hasRegisteredImages3d();
    EXPECT_TRUE(retVal);
}

TEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage2dArrayThenTransformerDidTransform) {
    bool retVal;
    retVal = imageTransformer.didTransform();
    EXPECT_FALSE(retVal);
    imageTransformer.transformImagesTo2dArray(*pKernelInfo, kernelArguments, nullptr);
    retVal = imageTransformer.didTransform();
    EXPECT_TRUE(retVal);
}

TEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage3dThenTransformerDidNotTransform) {
    bool retVal;
    retVal = imageTransformer.didTransform();
    EXPECT_FALSE(retVal);
    imageTransformer.transformImagesTo2dArray(*pKernelInfo, kernelArguments, nullptr);
    imageTransformer.transformImagesTo3d(*pKernelInfo, kernelArguments, nullptr);
    retVal = imageTransformer.didTransform();
    EXPECT_FALSE(retVal);
}
HWTEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage2dArrayThenTransformOnlyRegisteredImages) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    firstSurfaceState->setSurfaceArray(false);
    secondSurfaceState->setSurfaceArray(false);

    imageTransformer.registerImage3d(1);
    imageTransformer.transformImagesTo2dArray(*pKernelInfo, kernelArguments, ssh);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL, firstSurfaceState->getSurfaceType());
    EXPECT_FALSE(firstSurfaceState->getSurfaceArray());

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, secondSurfaceState->getSurfaceType());
    EXPECT_TRUE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage2dArrayThenTransformOnlyTransformableImages) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    pKernelInfo->argAt(1).getExtendedTypeInfo().isTransformable = false;

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    firstSurfaceState->setSurfaceArray(false);
    secondSurfaceState->setSurfaceArray(false);

    imageTransformer.registerImage3d(0);
    imageTransformer.registerImage3d(1);
    imageTransformer.transformImagesTo2dArray(*pKernelInfo, kernelArguments, ssh);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage3dThenTransformAllRegisteredImages) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    pKernelInfo->argAt(1).getExtendedTypeInfo().isTransformable = false;

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    firstSurfaceState->setSurfaceArray(true);
    secondSurfaceState->setSurfaceArray(true);

    imageTransformer.registerImage3d(0);
    imageTransformer.registerImage3d(1);
    imageTransformer.transformImagesTo3d(*pKernelInfo, kernelArguments, ssh);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, firstSurfaceState->getSurfaceType());
    EXPECT_FALSE(firstSurfaceState->getSurfaceArray());

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

HWTEST_F(ImageTransformerTest, givenImageTransformerWhenTransformToImage3dThenTransformOnlyRegisteredImages) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    pKernelInfo->argAt(1).getExtendedTypeInfo().isTransformable = false;

    auto firstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, firstImageOffset));
    auto secondSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh, secondImageOffset));

    firstSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    secondSurfaceState->setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL);
    firstSurfaceState->setSurfaceArray(true);
    secondSurfaceState->setSurfaceArray(true);

    imageTransformer.registerImage3d(1);
    imageTransformer.transformImagesTo3d(*pKernelInfo, kernelArguments, ssh);

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_NULL, firstSurfaceState->getSurfaceType());
    EXPECT_TRUE(firstSurfaceState->getSurfaceArray());

    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, secondSurfaceState->getSurfaceType());
    EXPECT_FALSE(secondSurfaceState->getSurfaceArray());
}

class MockImageTransformer : public ImageTransformer {
  public:
    using ImageTransformer::argIndexes;
};
TEST(ImageTransformerRegisterImageTest, givenImageTransformerWhenRegisterTheSameImageTwiceThenAppendOnlyOne) {
    MockImageTransformer transformer;
    EXPECT_EQ(0u, transformer.argIndexes.size());
    transformer.registerImage3d(0);
    EXPECT_EQ(1u, transformer.argIndexes.size());
    transformer.registerImage3d(0);
    EXPECT_EQ(1u, transformer.argIndexes.size());
    transformer.registerImage3d(1);
    EXPECT_EQ(2u, transformer.argIndexes.size());
}
