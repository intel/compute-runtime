/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClEnqueueFillImageTests = ApiTests;

namespace ULT {

TEST_F(ClEnqueueFillImageTests, GivenNullCommandQueueWhenFillingImageThenInvalidCommandQueueErrorIsReturned) {
    auto image = std::unique_ptr<Image>(Image2dHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(pContext));
    uint32_t fillColor[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {2, 2, 1};

    retVal = clEnqueueFillImage(
        nullptr,
        image.get(),
        fillColor,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(ClEnqueueFillImageTests, GivenNullImageWhenFillingImageThenInvalidMemObjectErrorIsReturned) {
    uint32_t fillColor[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {2, 2, 1};

    retVal = clEnqueueFillImage(
        pCommandQueue,
        nullptr,
        fillColor,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(ClEnqueueFillImageTests, GivenNullFillColorWhenFillingImageThenInvalidValueErrorIsReturned) {
    auto image = std::unique_ptr<Image>(Image2dHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(pContext));
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {2, 2, 1};

    retVal = clEnqueueFillImage(
        pCommandQueue,
        image.get(),
        nullptr,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClEnqueueFillImageTests, GivenCorrectArgumentsWhenFillingImageThenSuccessIsReturned) {
    auto image = std::unique_ptr<Image>(Image2dHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(pContext));
    uint32_t fillColor[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {2, 2, 1};

    retVal = clEnqueueFillImage(
        pCommandQueue,
        image.get(),
        fillColor,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(ClEnqueueFillImageTests, GivenQueueIncapableWhenFillingImageThenInvalidOperationReturned) {
    auto image = std::unique_ptr<Image>(Image2dHelperUlt<ImageUseHostPtr<Image2dDefaults>>::create(pContext));
    uint32_t fillColor[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {2, 2, 1};

    this->disableQueueCapabilities(CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL);
    retVal = clEnqueueFillImage(
        pCommandQueue,
        image.get(),
        fillColor,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}
} // namespace ULT
