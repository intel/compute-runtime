/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/context/context.h"
#include "runtime/sampler/sampler.h"

#include "CL/cl_ext.h"
#include "cl_api_tests.h"

using namespace NEO;

namespace ULT {

struct SamplerWithPropertiesTest : public api_fixture,
                                   public ::testing::WithParamInterface<std::tuple<uint64_t /*cl_sampler_properties*/, uint64_t /*cl_sampler_properties*/, uint64_t /*cl_sampler_properties*/>>,
                                   public ::testing::Test {
    SamplerWithPropertiesTest() {
    }

    void SetUp() override {
        std::tie(NormalizdProperties, AddressingProperties, FilterProperties) = GetParam();
        api_fixture::SetUp();
    }

    void TearDown() override {
        api_fixture::TearDown();
    }

    cl_sampler_properties NormalizdProperties = 0;
    cl_sampler_properties AddressingProperties = 0;
    cl_sampler_properties FilterProperties = 0;
};

typedef api_tests clCreateSamplerWithPropertiesTests;
typedef SamplerWithPropertiesTest clCreateSamplerWithProperties_;

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

TEST_P(clCreateSamplerWithProperties_, GivenCorrectParametersWhenCreatingSamplerWithPropertiesThenSamplerIsCreatedAndSuccessIsReturned) {
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

TEST_P(clCreateSamplerWithProperties_, GivenInvalidPropertiesWhenCreatingSamplerWithPropertiesThenInvalidValueErrorIsReturned) {
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
                        clCreateSamplerWithProperties_,
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
