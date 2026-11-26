/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flush_caches_bitmask.h"
#include "shared/test/common/debug_settings/debug_settings_manager_fixture.h"
#include "shared/test/unit_test/helpers/read_debug_settings_from_config_fixture.h"

namespace NEO {

TEST_F(ReadDebugSettingsFromConfigTest, givenFullyEnabledDebugManagerWhenReadingDebugSettingsFromConfigThenDebugAndReleaseVariablesAreReadWithoutWarning) {
    FullyEnabledTestDebugManager settingsManager;

    auto output = readDebugSettingsAndCaptureStdout(settingsManager);

    EXPECT_TRUE(settingsManager.flags.PrintDebugMessages.get());
    EXPECT_EQ(1, settingsManager.flags.EnableLEO.get());
    EXPECT_TRUE(output.empty());
}

TEST_F(ReadDebugSettingsFromConfigTest, givenFullyEnabledDebugManagerWhenReadingDebugSettingsFromConfigThenReadSettingsAreTranslated) {
    FullyEnabledTestDebugManager settingsManager;

    readDebugSettingsAndCaptureStdout(settingsManager);

    EXPECT_EQ(FlushCachesBitmask::allCaches, settingsManager.flags.FlushAllCaches.get());
}

TEST_F(ReadDebugSettingsFromConfigTest, givenDisabledDebugManagerWhenReadingDebugSettingsFromConfigThenOnlyReleaseVariablesAreReadAndWarningIsPrinted) {
    FullyDisabledTestDebugManager settingsManager;

    auto output = readDebugSettingsAndCaptureStdout(settingsManager);

    EXPECT_FALSE(settingsManager.flags.PrintDebugMessages.get());
    EXPECT_EQ(1, settingsManager.flags.EnableLEO.get());
    EXPECT_EQ("--read-config: debug variables are not supported in this build type, only release variables are read\n", output);
}

} // namespace NEO
