/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"

#include <functional>

using namespace NEO;

using ImageEnqueueCall = std::function<void(MockCommandQueue *, Image *, size_t *, size_t *, int32_t &)>;

struct ValidateRegionAndOriginTests : public ::testing::TestWithParam<ImageEnqueueCall> {
    void SetUp() override {
        context.reset(new MockContext());
        cmdQ.reset(new MockCommandQueue(context.get(), context->getDevice(0), 0));
    }

    static void readImage(MockCommandQueue *cmdQ, Image *image, size_t *origin, size_t *region, int32_t &retVal) {
        uint32_t tempPtr;
        retVal = clEnqueueReadImage(cmdQ, image, CL_TRUE, origin, region, 0, 0, &tempPtr, 0, nullptr, nullptr);
    }

    static void writeImage(MockCommandQueue *cmdQ, Image *image, size_t *origin, size_t *region, int32_t &retVal) {
        uint32_t tempPtr;
        retVal = clEnqueueWriteImage(cmdQ, image, CL_TRUE, origin, region, 0, 0, &tempPtr, 0, nullptr, nullptr);
    }

    static void fillImage(MockCommandQueue *cmdQ, Image *image, size_t *origin, size_t *region, int32_t &retVal) {
        uint32_t fill_color[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
        retVal = clEnqueueFillImage(cmdQ, image, fill_color, origin, region, 0, nullptr, nullptr);
    }

    static void copyImageWithCorrectSrc(MockCommandQueue *cmdQ, Image *dstImage, size_t *dstOrigin, size_t *region, int32_t &retVal) {
        std::unique_ptr<Image> srcImage(ImageHelper<Image3dDefaults>::create(&cmdQ->getContext()));
        size_t srcOrigin[3] = {0, 0, 0};
        retVal = clEnqueueCopyImage(cmdQ, srcImage.get(), dstImage, srcOrigin, dstOrigin, region, 0, nullptr, nullptr);
    }

    static void copyImageWithCorrectDst(MockCommandQueue *cmdQ, Image *srcImage, size_t *srcOrigin, size_t *region, int32_t &retVal) {
        std::unique_ptr<Image> dstImage(ImageHelper<Image3dDefaults>::create(&cmdQ->getContext()));
        size_t dstOrigin[3] = {0, 0, 0};
        retVal = clEnqueueCopyImage(cmdQ, srcImage, dstImage.get(), srcOrigin, dstOrigin, region, 0, nullptr, nullptr);
    }

    static void copyImageToBuffer(MockCommandQueue *cmdQ, Image *image, size_t *origin, size_t *region, int32_t &retVal) {
        MockBuffer buffer;
        retVal = clEnqueueCopyImageToBuffer(cmdQ, image, &buffer, origin, region, 0, 0, nullptr, nullptr);
    }

    static void copyBufferToImage(MockCommandQueue *cmdQ, Image *image, size_t *origin, size_t *region, int32_t &retVal) {
        MockBuffer buffer;
        retVal = clEnqueueCopyBufferToImage(cmdQ, &buffer, image, 0, origin, region, 0, nullptr, nullptr);
    }

    static void mapImage(MockCommandQueue *cmdQ, Image *image, size_t *origin, size_t *region, int32_t &retVal) {
        clEnqueueMapImage(cmdQ, image, CL_TRUE, CL_MAP_READ, origin, region, nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    }

    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockCommandQueue> cmdQ;
    cl_int retVal = CL_SUCCESS;
};

TEST_P(ValidateRegionAndOriginTests, givenAnyZeroRegionParamWhenEnqueueCalledThenReturnError) {
    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(context.get()));
    EXPECT_NE(nullptr, image.get());

    size_t origin[3] = {0, 0, 0};

    std::array<size_t, 3> region = {{0, 1, 1}};
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    region = {{1, 0, 1}};
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    region = {{1, 1, 0}};
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    region = {{0, 0, 0}};
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(ValidateRegionAndOriginTests, givenSecondOriginCoordinateAndNotAllowedImgTypeWhenEnqueueCalledThenReturnError) {
    size_t region[3] = {1, 1, 1};
    size_t origin[3] = {0, 1, 0};

    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(context.get()));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    auto image1dBufferDesc = Image1dDefaults::imageDesc;
    image1dBufferDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    image.reset(ImageHelper<Image1dDefaults>::create(context.get(), &image1dBufferDesc));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(ValidateRegionAndOriginTests, givenThirdOriginCoordinateAndNotAllowedImgTypeWhenEnqueueCalledThenReturnError) {
    size_t region[3] = {1, 1, 1};
    size_t origin[3] = {0, 0, 1};

    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(context.get()));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    image.reset(ImageHelper<Image2dDefaults>::create(context.get()));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    image.reset(ImageHelper<Image1dArrayDefaults>::create(context.get()));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    auto image1dBufferDesc = Image1dDefaults::imageDesc;
    image1dBufferDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    image.reset(ImageHelper<Image1dDefaults>::create(context.get(), &image1dBufferDesc));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(ValidateRegionAndOriginTests, givenSecondRegionCoordinateAndNotAllowedImgTypeWhenEnqueueCalledThenReturnError) {
    size_t region[3] = {1, 2, 1};
    size_t origin[3] = {0, 0, 0};

    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(context.get()));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    auto image1dBufferDesc = Image1dDefaults::imageDesc;
    image1dBufferDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    image.reset(ImageHelper<Image1dDefaults>::create(context.get(), &image1dBufferDesc));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(ValidateRegionAndOriginTests, givenThirdRegionnCoordinateAndNotAllowedImgTypeWhenEnqueueCalledThenReturnError) {
    size_t region[3] = {1, 1, 2};
    size_t origin[3] = {0, 0, 0};

    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(context.get()));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    image.reset(ImageHelper<Image2dDefaults>::create(context.get()));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    image.reset(ImageHelper<Image1dArrayDefaults>::create(context.get()));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    auto image1dBufferDesc = Image1dDefaults::imageDesc;
    image1dBufferDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    image.reset(ImageHelper<Image1dDefaults>::create(context.get(), &image1dBufferDesc));
    GetParam()(cmdQ.get(), image.get(), origin, &region[0], retVal);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

ImageEnqueueCall enqueueFunctions[8] = {
    &ValidateRegionAndOriginTests::readImage,
    &ValidateRegionAndOriginTests::writeImage,
    &ValidateRegionAndOriginTests::fillImage,
    &ValidateRegionAndOriginTests::copyImageWithCorrectSrc,
    &ValidateRegionAndOriginTests::copyImageWithCorrectDst,
    &ValidateRegionAndOriginTests::copyImageToBuffer,
    &ValidateRegionAndOriginTests::copyBufferToImage,
    &ValidateRegionAndOriginTests::mapImage,
};

INSTANTIATE_TEST_CASE_P(
    ValidateRegionAndOriginTests,
    ValidateRegionAndOriginTests,
    ::testing::ValuesIn(enqueueFunctions));
