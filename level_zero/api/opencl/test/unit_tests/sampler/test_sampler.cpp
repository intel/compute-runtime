/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/sampler/leo_sampler.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(RetainReleaseSamplerTests, givenNullSamplerWhenRetainSamplerThenReturnsCLInvalidSampler) {
    EXPECT_EQ(CL_INVALID_SAMPLER, clRetainSampler(nullptr));
}

TEST(RetainReleaseSamplerTests, givenNullSamplerWhenReleaseSamplerThenReturnsCLInvalidSampler) {
    EXPECT_EQ(CL_INVALID_SAMPLER, clReleaseSampler(nullptr));
}

TEST(GetSamplerInfoTests, givenNullSamplerWhenGetSamplerInfoThenReturnsCLInvalidSampler) {
    EXPECT_EQ(CL_INVALID_SAMPLER, clGetSamplerInfo(nullptr, CL_SAMPLER_CONTEXT, 0, nullptr, nullptr));
}

TEST(CreateSamplerTests, givenNullContextWhenCreateSamplerThenReturnsCLInvalidContext) {
    cl_int errcode = CL_SUCCESS;
    auto sampler = clCreateSampler(nullptr, CL_FALSE, CL_ADDRESS_NONE, CL_FILTER_NEAREST, &errcode);
    EXPECT_EQ(nullptr, sampler);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
}

TEST(CreateSamplerTests, givenNullContextWhenCreateSamplerWithPropertiesThenReturnsCLInvalidContext) {
    cl_int errcode = CL_SUCCESS;
    auto sampler = clCreateSamplerWithProperties(nullptr, nullptr, &errcode);
    EXPECT_EQ(nullptr, sampler);
    EXPECT_EQ(CL_INVALID_CONTEXT, errcode);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
