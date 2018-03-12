/*
* Copyright (c) 2018, Intel Corporation
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

#include "CL/cl.h"
#include "runtime/helpers/mipmap.h"
#include "gtest/gtest.h"

using namespace OCLRT;

typedef ::testing::TestWithParam<std::pair<size_t, uint32_t>> mipLevels;

const size_t expectedOrigin[] = {1, 2, 3, 4};

TEST_P(mipLevels, givenImageTypeReturnProperMipLevel) {
    auto pair = GetParam();
    EXPECT_EQ(pair.first, findMipLevel(pair.second, expectedOrigin));
}

INSTANTIATE_TEST_CASE_P(PatternType,
                        mipLevels,
                        ::testing::Values(std::make_pair(expectedOrigin[1], CL_MEM_OBJECT_IMAGE1D), std::make_pair(expectedOrigin[2], CL_MEM_OBJECT_IMAGE1D_ARRAY), std::make_pair(expectedOrigin[2], CL_MEM_OBJECT_IMAGE2D), std::make_pair(expectedOrigin[3], CL_MEM_OBJECT_IMAGE2D_ARRAY), std::make_pair(expectedOrigin[3], CL_MEM_OBJECT_IMAGE3D), std::make_pair(0u, 0u)));
