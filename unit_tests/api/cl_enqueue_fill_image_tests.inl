/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/ptr_math.h"
#include "runtime/command_queue/command_queue.h"
#include "unit_tests/fixtures/image_fixture.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueFillImageTests;

namespace ULT {

TEST_F(clEnqueueFillImageTests, GivenNullCommandQueueWhenFillingImageThenInvalidCommandQueueErrorIsReturned) {
    auto image = std::unique_ptr<Image>(Image2dHelper<ImageUseHostPtr<Image2dDefaults>>::create(pContext));
    uint32_t fill_color[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {2, 2, 1};

    retVal = clEnqueueFillImage(
        nullptr,
        image.get(),
        fill_color,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}

TEST_F(clEnqueueFillImageTests, GivenNullImageWhenFillingImageThenInvalidMemObjectErrorIsReturned) {
    uint32_t fill_color[4] = {0xaaaaaaaa, 0xbbbbbbbb, 0xcccccccc, 0xdddddddd};
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {2, 2, 1};

    retVal = clEnqueueFillImage(
        pCommandQueue,
        nullptr,
        fill_color,
        origin,
        region,
        0,
        nullptr,
        nullptr);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, retVal);
}

TEST_F(clEnqueueFillImageTests, GivenNullFillColorWhenFillingImageThenInvalidValueErrorIsReturned) {
    auto image = std::unique_ptr<Image>(Image2dHelper<ImageUseHostPtr<Image2dDefaults>>::create(pContext));
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
} // namespace ULT
