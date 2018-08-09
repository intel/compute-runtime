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

#include "cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/helpers/ptr_math.h"
#include "unit_tests/fixtures/image_fixture.h"

using namespace OCLRT;

typedef api_tests clEnqueueFillImageTests;

namespace ULT {

TEST_F(clEnqueueFillImageTests, nullCommandQueueReturnsError) {
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

TEST_F(clEnqueueFillImageTests, nullImageReturnsError) {
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

TEST_F(clEnqueueFillImageTests, nullFillColorReturnsError) {
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
