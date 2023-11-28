/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/destructor_counted.h"

namespace NEO {

using RootDeviceEnvironmentTests = ::testing::Test;

HWTEST2_F(RootDeviceEnvironmentTests, givenRootDeviceEnvironmentWhenAILInitProcessExecutableNameReturnsFailedThenInitAilConfigurationReturnsFail, IsDG1) {
    MockExecutionEnvironment executionEnvironment{};
    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());
    rootDeviceEnvironment->ailInitializationResult = false;

    EXPECT_EQ(false, rootDeviceEnvironment->initAilConfiguration());
}
} // namespace NEO
