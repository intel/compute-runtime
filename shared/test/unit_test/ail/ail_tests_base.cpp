/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using AILBaseTests = ::testing::Test;

HWTEST2_F(AILBaseTests, whenKernelSourceIsANGenDummyKernelThenDoEnforcePatchtokensFormat, IsAtLeastGen12lp) {
    std::string dummyKernelSource{"kernel void _(){}"};
    AILConfigurationHw<productFamily> ail;
    EXPECT_TRUE(ail.isFallbackToPatchtokensRequired(dummyKernelSource));
}

HWTEST2_F(AILBaseTests, whenKernelSourceIsNotANGenDummyKernelThenDoNotEnforcePatchtokensFormat, IsAtLeastGen12lp) {
    std::string dummyKernelSource{"kernel void copybuffer(__global int* a, __global int* b){ //some code }"};
    AILConfigurationHw<productFamily> ail;
    EXPECT_FALSE(ail.isFallbackToPatchtokensRequired(dummyKernelSource));
}

} // namespace NEO
