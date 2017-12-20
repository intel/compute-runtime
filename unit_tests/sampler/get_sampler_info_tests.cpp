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

#include "runtime/sampler/sampler.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(GetSamplerInfo, InvalidFlags_returnsError) {
    MockContext context;
    auto retVal = CL_INVALID_VALUE;
    auto normalizedCoords = CL_TRUE;
    auto addressingMode = CL_ADDRESS_MIRRORED_REPEAT;
    auto filterMode = CL_FILTER_NEAREST;
    auto sampler = Sampler::create(&context, normalizedCoords,
                                   addressingMode, filterMode, retVal);

    retVal = sampler->getInfo(0, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete sampler;
}

struct GetSamplerInfo : public ::testing::TestWithParam<uint32_t /*cl_sampler_info*/> {
    void SetUp() override {
        param = GetParam();
    }

    cl_sampler_info param;
};

TEST_P(GetSamplerInfo, valid_returnsSuccess) {
    MockContext context;
    auto retVal = CL_INVALID_VALUE;
    auto normalizedCoords = CL_TRUE;
    auto addressingMode = CL_ADDRESS_MIRRORED_REPEAT;
    auto filterMode = CL_FILTER_NEAREST;
    auto sampler = Sampler::create(&context, normalizedCoords, addressingMode,
                                   filterMode, retVal);

    size_t sizeReturned = 0;
    retVal = sampler->getInfo(param, 0, nullptr, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, retVal) << " param = " << param;
    ASSERT_NE(0u, sizeReturned);

    auto *object = new char[sizeReturned];
    retVal = sampler->getInfo(param, sizeReturned, object, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] object;
    delete sampler;
}

// Define new command types to run the parameterized tests
cl_sampler_info samplerInfoParams[] = {
    CL_SAMPLER_REFERENCE_COUNT,
    CL_SAMPLER_CONTEXT,
    CL_SAMPLER_NORMALIZED_COORDS,
    CL_SAMPLER_ADDRESSING_MODE,
    CL_SAMPLER_FILTER_MODE};

INSTANTIATE_TEST_CASE_P(
    Sampler_,
    GetSamplerInfo,
    testing::ValuesIn(samplerInfoParams));
