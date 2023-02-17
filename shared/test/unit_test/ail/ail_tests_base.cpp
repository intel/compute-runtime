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
    bool enforceRebuildToCTNI = false;

    AILConfigurationHw<productFamily> ail;
    ail.forceFallbackToPatchtokensIfRequired(dummyKernelSource, enforceRebuildToCTNI);

    EXPECT_TRUE(enforceRebuildToCTNI);
}

HWTEST2_F(AILBaseTests, whenKernelSourceIsNotANGenDummyKernelThenDoNotEnforcePatchtokensFormat, IsAtLeastSkl) {
    std::string dummyKernelSource{"kernel void copybuffer(__global int* a, __global int* b){ //some code }"};
    bool enforceRebuildToCTNI = false;

    AILConfigurationHw<productFamily> ail;
    ail.forceFallbackToPatchtokensIfRequired(dummyKernelSource, enforceRebuildToCTNI);

    EXPECT_FALSE(enforceRebuildToCTNI);
}

HWTEST2_F(AILBaseTests, givenResolveApplicationNameWhenCheckingIfPatchtokenFallbackIsRequiredThenIsCorrectResult, IsAtLeastSkl) {
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

        ail.applyExt(defaultHwInfo->capabilityTable);

        EXPECT_FALSE(defaultHwInfo->capabilityTable.blitterOperationsSupported);
    }
}

} // namespace NEO
