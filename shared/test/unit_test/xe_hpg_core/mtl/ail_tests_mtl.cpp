/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using AILTestsMTL = ::testing::Test;

HWTEST2_F(AILTestsMTL, givenMtlWhenSvchostAppIsDetectedThenDisableDirectSubmission, IsMTL) {
    AILWhitebox<productFamily> ail;

    auto capabilityTable = defaultHwInfo->capabilityTable;
    auto defaultEngineSupportedValue = capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported;

    ail.processName = "UnknownProcess";
    ail.apply(capabilityTable);
    EXPECT_EQ(defaultEngineSupportedValue, capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported);

    ail.processName = "svchost";
    ail.apply(capabilityTable);
    EXPECT_FALSE(capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported);

    ail.processName = "aomhost64";
    ail.apply(capabilityTable);
    EXPECT_FALSE(capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported);

    ail.processName = "Zoom";
    ail.apply(capabilityTable);
    EXPECT_FALSE(capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported);
}

HWTEST2_F(AILTestsMTL, givenApplicationNameRequiringCrossTargetCompabilityWhenCallingUseValidationLogicThenReturnProperValue, IsMTL) {
    AILWhitebox<productFamily> ail;
    ail.processName = "unknown";
    EXPECT_FALSE(ail.useLegacyValidationLogic());

    std::array<std::string_view, 3> applicationNamesRequiringLegacyValidation = {
        "blender", "bforartists", "cycles"};
    for (auto appName : applicationNamesRequiringLegacyValidation) {
        ail.processName = appName;
        EXPECT_TRUE(ail.useLegacyValidationLogic());
    }
}

} // namespace NEO
