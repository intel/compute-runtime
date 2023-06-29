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

using AILTestsMTL = ::testing::Test;

HWTEST2_F(AILTestsMTL, givenMtlWhenAdobeAppIsDetectedThenCCSItNotDefaultEngine, IsMTL) {
    class AILMock : public AILConfigurationHw<productFamily> {
      public:
        using AILConfiguration::apply;
        using AILConfiguration::processName;
    };

    VariableBackup<AILConfiguration *> ailConfigurationBackup(&ailConfigurationTable[productFamily]);
    AILMock ail;
    ailConfigurationTable[productFamily] = &ail;

    auto capabilityTable = defaultHwInfo->capabilityTable;

    for (auto &processName : {"Photoshop", "Adobe Premiere Pro", "AfterFX", "AfterFX (Beta)"}) {
        ail.processName = processName;
        capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS2;
        ail.apply(capabilityTable);
        EXPECT_EQ(aub_stream::ENGINE_RCS, capabilityTable.defaultEngineType);
    }

    ail.processName = "UnknownProcess";
    capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS3;
    ail.apply(capabilityTable);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, capabilityTable.defaultEngineType);
}
} // namespace NEO
