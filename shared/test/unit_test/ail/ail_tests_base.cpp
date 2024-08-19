/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using AILBaseTests = ::testing::Test;

HWTEST2_F(AILBaseTests, whenKernelSourceIsANGenDummyKernelThenDoEnforcePatchtokensFormat, IsAtLeastSkl) {
    std::string dummyKernelSource{"kernel void _(){}"};
    AILConfigurationHw<productFamily> ail;
    EXPECT_TRUE(ail.isFallbackToPatchtokensRequired(dummyKernelSource));
}

HWTEST2_F(AILBaseTests, whenKernelSourceIsNotANGenDummyKernelThenDoNotEnforcePatchtokensFormat, IsAtLeastSkl) {
    std::string dummyKernelSource{"kernel void copybuffer(__global int* a, __global int* b){ //some code }"};
    AILConfigurationHw<productFamily> ail;
    EXPECT_FALSE(ail.isFallbackToPatchtokensRequired(dummyKernelSource));
}

HWTEST2_F(AILBaseTests, givenApplicationNamesThatRequireAILWhenCheckingIfPatchtokenFallbackIsRequiredThenIsCorrectResult, IsAtLeastSkl) {
    AILWhitebox<productFamily> ail;
    for (const auto &name : {"ArcControlAssist",
                             "ArcControl"}) {
        ail.processName = name;
        EXPECT_TRUE(ail.isFallbackToPatchtokensRequired(""));
    }
}

HWTEST2_F(AILBaseTests, givenResolveNameWhenCheckingIfPatchtokenFallbackIsRequiredThenIsCorrectResult, IsAtMostXeHpcCore) {
    AILWhitebox<productFamily> ail;
    ail.processName = "Resolve";
    EXPECT_TRUE(ail.isFallbackToPatchtokensRequired(""));
}

} // namespace NEO
