/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/debug_settings/debug_settings_manager_fixture.h"
#include "shared/test/unit_test/helpers/read_debug_settings_from_config_fixture.h"

#include <unordered_map>

namespace NEO {

TEST_F(ReadDebugSettingsFromConfigTest, givenDisabledDebugManagerAndDebugKeysReadEnabledWhenReadingDebugSettingsFromConfigThenDebugVariablesAreReadWithoutWarning) {
    FullyDisabledTestDebugManager settingsManager;
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEOReadDebugKeys", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    auto output = readDebugSettingsAndCaptureStdout(settingsManager);

    EXPECT_TRUE(settingsManager.flags.PrintDebugMessages.get());
    EXPECT_EQ(1, settingsManager.flags.EnableLEO.get());
    EXPECT_TRUE(output.empty());
}

} // namespace NEO
