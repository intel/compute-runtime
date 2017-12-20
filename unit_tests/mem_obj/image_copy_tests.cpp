/*
 * Copyright (c) 2017, Intel Corporation
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

#include "runtime/mem_obj/image.h"
#include "gtest/gtest.h"

using namespace OCLRT;

const char valueOfEmptyPixel = 0;
const char valueOfCopiedPixel = 1;
const char garbageValue = 2;

const int imageCount = 2;
const int imageDimension = 25;
auto const elementSize = 4;

class CopyImageTest : public testing::WithParamInterface<std::tuple<size_t, size_t> /*srcPitch, destPitch*/>,
                      public testing::Test {

  public:
    void SetUp() override {
        std::tie(srcRowPitch, destRowPitch) = GetParam();
        // clang-format off
        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = imageDimension;
        imageDesc.image_height      = imageDimension;
        imageDesc.image_depth       = 0;
        imageDesc.image_array_size  = imageCount;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object = NULL;
        // clang-format on
        lineWidth = imageDesc.image_width * elementSize;
        srcSlicePitch = srcRowPitch * imageDimension;
        destSlicePitch = destRowPitch * imageDimension;
        srcPtrSize = srcSlicePitch * imageCount;
        destPtrSize = destSlicePitch * imageCount;
        srcPtr.reset(new char[srcPtrSize]);
        destPtr.reset(new char[destPtrSize * 2]);
    }

    cl_image_desc imageDesc;
    size_t srcRowPitch;
    size_t srcSlicePitch;
    size_t destRowPitch;
    size_t destSlicePitch;
    size_t srcPtrSize;
    size_t destPtrSize;
    size_t lineWidth;
    std::unique_ptr<char> srcPtr;
    std::unique_ptr<char> destPtr;
};

TEST_P(CopyImageTest, givenSrcAndDestPitchesWhenTransferDataIsCalledThenSpecificValuesAreCopied) {
    memset(destPtr.get(), valueOfEmptyPixel, 2 * destPtrSize);
    memset(srcPtr.get(), garbageValue, srcPtrSize);
    for (size_t i = 0; i < imageCount; ++i) {
        for (size_t j = 0; j < imageDimension; ++j) {
            memset(srcPtr.get() + i * srcSlicePitch + j * srcRowPitch, valueOfCopiedPixel, lineWidth);
        }
    }
    Image::transferData(srcPtr.get(), srcRowPitch, srcSlicePitch, destPtr.get(), destRowPitch, destSlicePitch, &imageDesc, elementSize, imageCount);
    size_t unconfirmedCopies = 0;
    //expect no garbage copied
    for (size_t i = 0; i < 2 * destPtrSize; ++i) {
        if (destPtr.get()[i] == valueOfCopiedPixel) {
            unconfirmedCopies++;
        }
        EXPECT_NE(garbageValue, destPtr.get()[i]);
    }
    //expect copied to right locations
    for (size_t i = 0; i < imageCount; ++i) {
        for (size_t j = 0; j < imageDimension; ++j) {
            for (size_t k = 0; k < lineWidth; ++k) {
                EXPECT_EQ(valueOfCopiedPixel, destPtr.get()[i * destSlicePitch + j * destRowPitch + k]);
                unconfirmedCopies--;
            }
        }
    }
    //expect copied only to destPtr
    for (size_t i = 0; i < destPtrSize; ++i) {
        EXPECT_EQ(valueOfEmptyPixel, destPtr.get()[destPtrSize + i]);
    }
    EXPECT_EQ(0u, unconfirmedCopies);
}
size_t valuesOfPitchesInCopyTests[] = {100, 101, 102};

INSTANTIATE_TEST_CASE_P(
    CopyImageTests,
    CopyImageTest,
    testing::Combine(
        testing::ValuesIn(valuesOfPitchesInCopyTests),
        testing::ValuesIn(valuesOfPitchesInCopyTests)));
