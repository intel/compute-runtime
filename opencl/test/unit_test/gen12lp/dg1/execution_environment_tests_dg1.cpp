/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/utilities/destructor_counted.h"

namespace NEO {

using RootDeviceEnvironmentTests = ::testing::Test;

HWTEST2_F(RootDeviceEnvironmentTests, givenRootDeviceEnvironmentWhenAILInitProcessExecutableNameReturnsFailedThenInitAilConfigurationReturnsFail, IsDG1) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    class AILDG1 : public AILConfigurationHw<productFamily> {
      public:
        bool initProcessExecutableName() override {
            return false;
        }
    };
    VariableBackup<AILConfiguration *> ailConfiguration(&ailConfigurationTable[productFamily]);

    AILDG1 ailDg1;
    ailConfigurationTable[productFamily] = &ailDg1;

    EXPECT_EQ(false, rootDeviceEnvironment->initAilConfiguration());
}
} // namespace NEO