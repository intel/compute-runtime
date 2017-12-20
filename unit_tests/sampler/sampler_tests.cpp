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
#include "unit_tests/mocks/mock_sampler.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "gtest/gtest.h"
#include "patch_list.h"
#include <tuple>

using namespace OCLRT;

struct CreateSampler : public MemoryManagementFixture,
                       public ::testing::TestWithParam<
                           std::tuple<uint32_t /*cl_bool*/, uint32_t /*cl_addressing_mode*/, uint32_t /*cl_filter_mode*/>> {
    CreateSampler() {
    }

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        std::tie(normalizedCoords, addressingMode, filterMode) = GetParam();
        context = new MockContext();
    }

    void TearDown() override {
        delete context;
        MemoryManagementFixture::TearDown();
    }

    // clang-format off
    MockContext * context;
    cl_int             retVal = CL_INVALID_VALUE;
    cl_bool            normalizedCoords;
    cl_addressing_mode addressingMode;
    cl_filter_mode     filterMode;
    // clang-format on
};

TEST_P(CreateSampler, shouldReturnSuccess) {
    auto sampler = Sampler::create(
        context,
        normalizedCoords,
        addressingMode,
        filterMode,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, sampler);
    delete sampler;
}

TEST_P(CreateSampler, shouldPropagateSamplerState) {
    auto sampler = new MockSampler(
        context,
        normalizedCoords,
        addressingMode,
        filterMode);
    ASSERT_NE(nullptr, sampler);

    // clang-format off
    EXPECT_EQ(context,         sampler->getContext());
    EXPECT_EQ(normalizedCoords, sampler->getNormalizedCoordinates());
    EXPECT_EQ(addressingMode,   sampler->getAddressingMode());
    EXPECT_EQ(filterMode,       sampler->getFilterMode());
    // clang-format on

    //check for SnapWA
    bool snapWaNeeded = addressingMode == CL_ADDRESS_CLAMP && filterMode == CL_FILTER_NEAREST;
    auto snapWaValue = snapWaNeeded ? iOpenCL::CONSTANT_REGISTER_BOOLEAN_TRUE : iOpenCL::CONSTANT_REGISTER_BOOLEAN_FALSE;
    EXPECT_EQ(snapWaValue, sampler->getSnapWaValue());

    delete sampler;
}

static cl_bool normalizedCoordModes[] = {
    CL_FALSE,
    CL_TRUE};

static cl_addressing_mode addressingModes[] = {
    CL_ADDRESS_MIRRORED_REPEAT,
    CL_ADDRESS_REPEAT,
    CL_ADDRESS_CLAMP_TO_EDGE,
    CL_ADDRESS_CLAMP,
    CL_ADDRESS_NONE};

static cl_filter_mode filterModes[] = {
    CL_FILTER_NEAREST,
    CL_FILTER_LINEAR};

INSTANTIATE_TEST_CASE_P(Sampler,
                        CreateSampler,
                        ::testing::Combine(
                            ::testing::ValuesIn(normalizedCoordModes),
                            ::testing::ValuesIn(addressingModes),
                            ::testing::ValuesIn(filterModes)));
