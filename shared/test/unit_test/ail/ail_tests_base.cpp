/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
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

HWTEST2_F(AILBaseTests, givenApplicationNamesThatRequirAILWhenCheckingIfPatchtokenFallbackIsRequiredThenIsCorrectResult, IsAtLeastSkl) {
    class AILMock : public AILConfigurationHw<productFamily> {
      public:
        using AILConfiguration::processName;
    };

    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);
    AILMock ail;
    ailConfigurationTable[productFamily] = &ail;

    for (const auto &name : {"Resolve",
                             "ArcControlAssist",
                             "ArcControl"}) {
        ail.processName = name;
        EXPECT_TRUE(ail.isFallbackToPatchtokensRequired(""));
    }
}

} // namespace NEO
