/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/source/sampler/sampler.h"

#include "CL/cl_ext.h"
#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct SamplerWithPropertiesTest : public ApiFixture<>,
                                   public ::testing::WithParamInterface<std::tuple<uint64_t /*cl_sampler_properties*/, uint64_t /*cl_sampler_properties*/, uint64_t /*cl_sampler_properties*/>>,
                                   public ::testing::Test {
    SamplerWithPropertiesTest() {
    }

    void SetUp() override {
        std::tie(NormalizdProperties, AddressingProperties, FilterProperties) = GetParam();
        ApiFixture::SetUp();
    }

    void TearDown() override {
        ApiFixture::TearDown();
    }

    cl_sampler_properties NormalizdProperties = 0;
    cl_sampler_properties AddressingProperties = 0;
    cl_sampler_properties FilterProperties = 0;
};

typedef api_tests clCreateSamplerWithPropertiesTests;
typedef SamplerWithPropertiesTest clCreateSamplerWithPropertiesTests2;

TEST_F(clCreateSamplerWithPropertiesTests, GivenSamplerPropertiesAndNoReturnPointerWhenCreatingSamplerWithPropertiesThenSamplerIsCreated) {
    cl_sampler sampler = nullptr;
    cl_queue_properties properties[] =
        {
            CL_SAMPLER_NORMALIZED_COORDS, 0,
            CL_SAMPLER_ADDRESSING_MODE, CL_ADDRESS_NONE,
            CL_SAMPLER_FILTER_MODE, CL_FILTER_LINEAR,
            0};

    sampler = clCreateSamplerWithProperties(
        pContext,
        properties,
        nullptr);
    ASSERT_NE(nullptr, sampler);

    retVal = clReleaseSampler(sampler);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clCreateSamplerWithPropertiesTests, GivenNullContextWhenCreatingSamplerWithPropertiesThenInvalidContextErrorIsReturned) {
    cl_sampler sampler = nullptr;
    cl_queue_properties properties[] =
        {
            CL_SAMPLER_NORMALIZED_COORDS, 0,
            CL_SAMPLER_ADDRESSING_MODE, CL_ADDRESS_NONE,
            CL_SAMPLER_FILTER_MODE, CL_FILTER_LINEAR,
            0};

    sampler = clCreateSamplerWithProperties(
        nullptr,
        properties,
        &retVal);
    ASSERT_EQ(nullptr, sampler);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clCreateSamplerWithPropertiesTests, GivenSamplerCreatedWithNullPropertiesWhenQueryingPropertiesThenNothingIsReturned) {
    cl_int retVal = CL_SUCCESS;
    auto sampler = clCreateSamplerWithProperties(pContext, nullptr, &retVal);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_NE(nullptr, sampler);

    size_t propertiesSize;
    retVal = clGetSamplerInfo(sampler, CL_SAMPLER_PROPERTIES, 0, nullptr, &propertiesSize);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(0u, propertiesSize);

    clReleaseSampler(sampler);
}

TEST_F(clCreateSamplerWithPropertiesTests, WhenCreatingSamplerWithPropertiesThenPropertiesAreCorrectlyStored) {
    cl_int retVal = CL_SUCCESS;
    cl_sampler_properties properties[7];
    size_t propertiesSize;

    std::vector<std::vector<uint64_t>> propertiesToTest{
        {0},
        {CL_SAMPLER_FILTER_MODE, CL_FILTER_LINEAR, 0},
        {CL_SAMPLER_NORMALIZED_COORDS, 0, CL_SAMPLER_ADDRESSING_MODE, CL_ADDRESS_NONE, CL_SAMPLER_FILTER_MODE, CL_FILTER_LINEAR, 0}};

    for (auto testProperties : propertiesToTest) {
        auto sampler = clCreateSamplerWithProperties(pContext, testProperties.data(), &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, sampler);

        retVal = clGetSamplerInfo(sampler, CL_SAMPLER_PROPERTIES, sizeof(properties), properties, &propertiesSize);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(testProperties.size() * sizeof(cl_sampler_properties), propertiesSize);
        for (size_t i = 0; i < testProperties.size(); i++) {
            EXPECT_EQ(testProperties[i], properties[i]);
        }

        clReleaseSampler(sampler);
    }
}

TEST_P(clCreateSamplerWithPropertiesTests2, GivenCorrectParametersWhenCreatingSamplerWithPropertiesThenSamplerIsCreatedAndSuccessIsReturned) {
    cl_sampler sampler = nullptr;
    cl_queue_properties properties[] =
        {
            CL_SAMPLER_NORMALIZED_COORDS, 0,
            CL_SAMPLER_ADDRESSING_MODE, 0,
            CL_SAMPLER_FILTER_MODE, 0,
            0};

    cl_queue_properties *pProp = &properties[0];
    if (NormalizdProperties) {
        *pProp++ = CL_SAMPLER_NORMALIZED_COORDS;
        *pProp++ = (cl_queue_properties)NormalizdProperties;
    }

    if (AddressingProperties) {
        *pProp++ = CL_SAMPLER_ADDRESSING_MODE;
        *pProp++ = (cl_queue_properties)AddressingProperties;
    }

    if (FilterProperties) {
        *pProp++ = CL_SAMPLER_FILTER_MODE;
        *pProp++ = (cl_queue_properties)FilterProperties;
    }

    *pProp++ = 0;

    sampler = clCreateSamplerWithProperties(
        pContext,
        properties,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, sampler);

    retVal = clReleaseSampler(sampler);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(clCreateSamplerWithPropertiesTests2, GivenInvalidPropertiesWhenCreatingSamplerWithPropertiesThenInvalidValueErrorIsReturned) {
    cl_sampler sampler = nullptr;
    cl_queue_properties properties[] =
        {
            CL_SAMPLER_NORMALIZED_COORDS, 0,
            CL_SAMPLER_NORMALIZED_COORDS, 0,
            0};

    cl_queue_properties *pProp = &properties[0];
    if (NormalizdProperties) {
        *pProp++ = CL_SAMPLER_NORMALIZED_COORDS;
        *pProp++ = (cl_queue_properties)NormalizdProperties;
        *pProp++ = CL_SAMPLER_NORMALIZED_COORDS;
        *pProp++ = (cl_queue_properties)NormalizdProperties;

        *pProp++ = 0;

        sampler = clCreateSamplerWithProperties(
            pContext,
            properties,
            &retVal);
        EXPECT_EQ(CL_INVALID_VALUE, retVal);
        ASSERT_EQ(nullptr, sampler);
    }

    pProp = &properties[0];
    if (AddressingProperties) {
        *pProp++ = CL_SAMPLER_ADDRESSING_MODE;
        *pProp++ = (cl_queue_properties)AddressingProperties;
        *pProp++ = CL_SAMPLER_ADDRESSING_MODE;
        *pProp++ = (cl_queue_properties)AddressingProperties;

        *pProp++ = 0;

        sampler = clCreateSamplerWithProperties(
            pContext,
            properties,
            &retVal);
        EXPECT_EQ(CL_INVALID_VALUE, retVal);
        ASSERT_EQ(nullptr, sampler);
    }

    pProp = &properties[0];
    if (FilterProperties) {
        *pProp++ = CL_SAMPLER_FILTER_MODE;
        *pProp++ = (cl_queue_properties)FilterProperties;
        *pProp++ = CL_SAMPLER_FILTER_MODE;
        *pProp++ = (cl_queue_properties)FilterProperties;

        *pProp++ = 0;

        sampler = clCreateSamplerWithProperties(
            pContext,
            properties,
            &retVal);
        EXPECT_EQ(CL_INVALID_VALUE, retVal);
        ASSERT_EQ(nullptr, sampler);
    }
}

static cl_sampler_properties NormalizdProperties[] =
    {
        CL_TRUE,
        CL_FALSE,
};

static cl_sampler_properties AddressingProperties[] =
    {
        CL_ADDRESS_NONE,
        CL_ADDRESS_CLAMP_TO_EDGE,
        CL_ADDRESS_CLAMP,
        CL_ADDRESS_REPEAT,
        CL_ADDRESS_MIRRORED_REPEAT,
};

static cl_sampler_properties FilterProperties[] =
    {
        CL_FILTER_NEAREST,
        CL_FILTER_LINEAR,
};

INSTANTIATE_TEST_CASE_P(api,
                        clCreateSamplerWithPropertiesTests2,
                        ::testing::Combine(
                            ::testing::ValuesIn(NormalizdProperties),
                            ::testing::ValuesIn(AddressingProperties),
                            ::testing::ValuesIn(FilterProperties)));

TEST_F(clCreateSamplerWithPropertiesTests, GivenMipMapDataWhenCreatingSamplerWithPropertiesThenSamplerIsCreatedAndCorrectlyPopulated) {
    SamplerLodProperty minLodProperty;
    SamplerLodProperty maxLodProperty;
    minLodProperty.lod = 2.0f;
    maxLodProperty.lod = 3.0f;
    cl_sampler_properties mipMapFilteringMode = CL_FILTER_LINEAR;
    cl_sampler_properties properties[] =
        {
            CL_SAMPLER_NORMALIZED_COORDS, 0,
            CL_SAMPLER_ADDRESSING_MODE, CL_ADDRESS_NONE,
            CL_SAMPLER_FILTER_MODE, CL_FILTER_LINEAR,
            CL_SAMPLER_MIP_FILTER_MODE, mipMapFilteringMode,
            CL_SAMPLER_LOD_MIN, minLodProperty.data,
            CL_SAMPLER_LOD_MAX, maxLodProperty.data,
            0};

    cl_sampler clSampler = clCreateSamplerWithProperties(
        pContext,
        properties,
        &retVal);

    auto sampler = castToObject<Sampler>(clSampler);
    ASSERT_NE(nullptr, sampler);
    EXPECT_EQ(mipMapFilteringMode, sampler->mipFilterMode);
    EXPECT_EQ(minLodProperty.lod, sampler->lodMin);
    EXPECT_EQ(maxLodProperty.lod, sampler->lodMax);
    clReleaseSampler(sampler);
}

} // namespace ULT
