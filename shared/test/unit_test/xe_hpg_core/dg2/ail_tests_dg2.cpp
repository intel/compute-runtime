/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

using AILTestsDg2 = ::testing::Test;

HWTEST2_F(AILTestsDg2, givenFixesForApplicationsWhenModifyKernelIfRequiredIsCalledThenReturnCorrectResults, IsDG2) {

    class AILMock : public AILConfigurationHw<productFamily> {
      public:
        using AILConfiguration::processName;
        using AILConfiguration::sourcesContain;

        bool isKernelHashCorrect(const std::string &kernelSources, uint64_t expectedHash) const override {
            return hashCorrect;
        }

        bool hashCorrect = {true};
    };

    AILMock ail;
    std::string_view fixCode = "else { SYNC_WARPS; }";

    for (auto name : {"FAHBench-gui", "FAHBench-cmd"}) {
        {
            ail.processName = name;
            ail.hashCorrect = true;

            // sources don't contain kernel name
            std::string kernelSources;
            kernelSources.resize(16480u, 'a');
            auto copyKernelSources = kernelSources;

            EXPECT_FALSE(ail.sourcesContain(kernelSources, "findBlocksWithInteractions"));

            // sources should not change
            ail.modifyKernelIfRequired(kernelSources);
            EXPECT_STREQ(kernelSources.c_str(), copyKernelSources.c_str());

            // sources should not contain extra synchronization
            auto it = kernelSources.find(fixCode);
            EXPECT_EQ(it, std::string::npos);
        }
        {
            // sources contain kernel name
            std::string kernelSources;
            kernelSources.resize(16480u, 'a');
            kernelSources.insert(1024u, "findBlocksWithInteractions");
            auto copyKernelSources = kernelSources;
            EXPECT_TRUE(ail.sourcesContain(kernelSources, "findBlocksWithInteractions"));

            // sources should change
            ail.modifyKernelIfRequired(kernelSources);
            EXPECT_STRNE(kernelSources.c_str(), copyKernelSources.c_str());

            // sources should contain extra synchronization
            auto it = kernelSources.find(fixCode);
            EXPECT_NE(it, std::string::npos);

            constexpr auto expectedFixStartPosition = 12651u;
            EXPECT_EQ(expectedFixStartPosition, it);
        }
        {
            // hash doesn't match
            ail.hashCorrect = false;

            // sources  contain kernel name
            std::string kernelSources;
            kernelSources.resize(16480u, 'a');
            kernelSources.insert(1024u, "findBlocksWithInteractions");
            auto copyKernelSources = kernelSources;

            EXPECT_TRUE(ail.sourcesContain(kernelSources, "findBlocksWithInteractions"));

            // sources should not change
            ail.modifyKernelIfRequired(kernelSources);
            EXPECT_STREQ(kernelSources.c_str(), copyKernelSources.c_str());

            // sources should not contain extra synchronization
            auto it = kernelSources.find(fixCode);
            EXPECT_EQ(it, std::string::npos);
        }
    }

    {
        // sources should not change for non-existent application
        ail.processName = "nonExistentApplication";
        ail.hashCorrect = true;
        std::string kernelSources = "example_kernel(){}";
        auto copyKernelSources = kernelSources;
        ail.modifyKernelIfRequired(kernelSources);

        EXPECT_STREQ(copyKernelSources.c_str(), kernelSources.c_str());
    }
}

HWTEST2_F(AILTestsDg2, givenApplicationNameRequiringCrossTargetCompabilityWhenCallingUseValidationLogicThenReturnProperValue, IsDG2) {
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
