/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_sampler.h"

#include "gtest/gtest.h"

using namespace NEO;

struct CreateSampler : public ::testing::Test {
    MockContext context;
    cl_int retVal = CL_INVALID_VALUE;
};

TEST_F(CreateSampler, WhenSamplerIsCreatedThenSuccessIsReturned) {
    const cl_bool normalizedCoordModes[] = {CL_FALSE, CL_TRUE};
    const cl_addressing_mode addressingModes[] = {CL_ADDRESS_MIRRORED_REPEAT, CL_ADDRESS_REPEAT,
                                                  CL_ADDRESS_CLAMP_TO_EDGE, CL_ADDRESS_CLAMP, CL_ADDRESS_NONE};
    const cl_filter_mode filterModes[] = {CL_FILTER_NEAREST, CL_FILTER_LINEAR};
    for (auto normalizedCoords : normalizedCoordModes) {
        for (auto addressingMode : addressingModes) {
            for (auto filterMode : filterModes) {
                retVal = CL_INVALID_VALUE;
                std::unique_ptr<Sampler> sampler(Sampler::create(&context, normalizedCoords, addressingMode, filterMode, retVal));
                EXPECT_EQ(CL_SUCCESS, retVal);
                EXPECT_NE(nullptr, sampler);
            }
        }
    }
}

TEST_F(CreateSampler, GivenModeWhenSamplerIsCreatedThenParamsAreSetCorrectly) {
    const cl_bool normalizedCoordModes[] = {CL_FALSE, CL_TRUE};
    const cl_addressing_mode addressingModes[] = {CL_ADDRESS_MIRRORED_REPEAT, CL_ADDRESS_REPEAT,
                                                  CL_ADDRESS_CLAMP_TO_EDGE, CL_ADDRESS_CLAMP, CL_ADDRESS_NONE};
    const cl_filter_mode filterModes[] = {CL_FILTER_NEAREST, CL_FILTER_LINEAR};
    for (auto normalizedCoords : normalizedCoordModes) {
        for (auto addressingMode : addressingModes) {
            for (auto filterMode : filterModes) {
                std::unique_ptr<MockSampler> sampler(new MockSampler(&context, normalizedCoords, addressingMode, filterMode));
                ASSERT_NE(nullptr, sampler);

                EXPECT_EQ(&context, sampler->getContext());
                EXPECT_EQ(normalizedCoords, sampler->getNormalizedCoordinates());
                EXPECT_EQ(addressingMode, sampler->getAddressingMode());
                EXPECT_EQ(filterMode, sampler->getFilterMode());

                bool snapWaNeeded = addressingMode == CL_ADDRESS_CLAMP && filterMode == CL_FILTER_NEAREST;
                EXPECT_EQ(snapWaNeeded, sampler->getSnapWaValue() != 0u);
            }
        }
    }
}

TEST(castToSamplerTest, GivenGenericPointerWhichHoldsSamplerObjectWhenCastToSamplerIsCalledThenCastWithSuccess) {
    cl_int retVal;
    auto context = std::make_unique<MockContext>();
    cl_sampler clSampler = Sampler::create(
        context.get(),
        CL_TRUE,
        CL_ADDRESS_MIRRORED_REPEAT,
        CL_FILTER_NEAREST,
        retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    auto ptr = reinterpret_cast<void *>(clSampler);
    auto sampler = castToObject<Sampler>(ptr);

    EXPECT_NE(nullptr, sampler);
    clReleaseSampler(clSampler);
}

TEST(castToSamplerTest, GivenGenericPointerWhichDoestNotHoldSamplerObjectWhenCastToSamplerIsCalledThenCastWithAFailure) {
    auto context = std::make_unique<MockContext>();
    auto notSamplerObj = std::unique_ptr<Image>(ImageHelperUlt<Image2dDefaults>::create(context.get()));
    void *ptr = notSamplerObj.get();
    auto notSampler = castToObject<Sampler>(ptr);

    EXPECT_EQ(nullptr, notSampler);
}
