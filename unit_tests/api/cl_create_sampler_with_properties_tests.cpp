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
#include "runtime/context/context.h"
#include "runtime/sampler/sampler.h"
#include "CL/cl_ext.h"

using namespace OCLRT;

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

TEST_F(clCreateSamplerWithPropertiesTests, givenSamplerPropertiesWhenReturnValueIsNotPassedThenSamplerIsCreated) {
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

TEST_F(clCreateSamplerWithPropertiesTests, nullContext) {
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
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_P(clCreateSamplerWithProperties_, returnsSuccess) {
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

TEST_P(clCreateSamplerWithProperties_, returnsFailure) {
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

TEST_F(clCreateSamplerWithPropertiesTests, whenCreatedWithMipMapDataThenSamplerIsProperlyPopulated) {
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
