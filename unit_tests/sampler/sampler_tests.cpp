/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sampler/sampler.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_sampler.h"

#include "gtest/gtest.h"
#include "patch_list.h"

#include <tuple>

using namespace OCLRT;

struct CreateSampler : public ::testing::TestWithParam<
                           std::tuple<uint32_t /*cl_bool*/, uint32_t /*cl_addressing_mode*/, uint32_t /*cl_filter_mode*/>> {
    CreateSampler() {
    }

    void SetUp() override {
        std::tie(normalizedCoords, addressingMode, filterMode) = GetParam();
        context = new MockContext();
    }

    void TearDown() override {
        delete context;
    }

    MockContext *context;
    cl_int retVal = CL_INVALID_VALUE;
    cl_bool normalizedCoords;
    cl_addressing_mode addressingMode;
    cl_filter_mode filterMode;
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

    EXPECT_EQ(context, sampler->getContext());
    EXPECT_EQ(normalizedCoords, sampler->getNormalizedCoordinates());
    EXPECT_EQ(addressingMode, sampler->getAddressingMode());
    EXPECT_EQ(filterMode, sampler->getFilterMode());

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

typedef ::testing::TestWithParam<std::tuple<uint32_t /*normalizedCoords*/, uint32_t /*addressingMode*/, uint32_t /*filterMode*/>> TransformableSamplerTest;

TEST_P(TransformableSamplerTest, givenSamplerWhenHasProperParametersThenIsTransformable) {
    bool expectedRetVal;
    bool retVal;
    cl_bool normalizedCoords;
    cl_addressing_mode addressingMode;
    cl_filter_mode filterMode;
    std::tie(normalizedCoords, addressingMode, filterMode) = GetParam();

    expectedRetVal = addressingMode == CL_ADDRESS_CLAMP_TO_EDGE &&
                     filterMode == CL_FILTER_NEAREST &&
                     normalizedCoords == CL_FALSE;

    MockSampler sampler(nullptr, normalizedCoords, addressingMode, filterMode);

    retVal = sampler.isTransformable();
    EXPECT_EQ(expectedRetVal, retVal);
}
INSTANTIATE_TEST_CASE_P(Sampler,
                        TransformableSamplerTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(normalizedCoordModes),
                            ::testing::ValuesIn(addressingModes),
                            ::testing::ValuesIn(filterModes)));

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
    auto notSamplerObj = std::unique_ptr<Image>(ImageHelper<Image2dDefaults>::create(context.get()));
    void *ptr = notSamplerObj.get();
    auto notSampler = castToObject<Sampler>(ptr);

    EXPECT_EQ(nullptr, notSampler);
}
