/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_variables_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/utilities/debug_file_reader.h"
#include "shared/test/common/debug_settings/debug_settings_manager_fixture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstring>
#include <unordered_map>

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;

TEST(DebugVariablesHelperTests, whenIsDebugKeysReadEnableIsCalledThenFalseIsReturned) {
    EXPECT_FALSE(NEO::isDebugKeysReadEnabled());
}

TEST(DebugSettingsManager, givenFileOrRegistryReaderActiveThenRegularVariablesFollowItWhileRawEnvVariablesAlwaysComeFromRealEnvironment) {
    // Plays the role of a neo.config/igdrcl.config settings file. Deliberately has no entry for
    // OverrideDefaultFP64Settings - unlike RegistryReader, SettingsFileReader has no fallback to the
    // environment for keys it doesn't contain.
    struct MockSettingFileReader : SettingsFileReader {
        MockSettingFileReader() : SettingsFileReader("") {
            settingStringMap["EnableLEO"] = "1";
            settingStringMap["ZE_AFFINITY_MASK"] = "0.5";
        }
    };

    // Mirrors RegistryReader's own two-tier behavior (shared/source/os_interface/windows/
    // debug_registry_reader.cpp): a real registry hit for EnableLEO only; every other key falls
    // through to the same environment check RegistryReader itself performs when the registry misses.
    struct MockRegistryReader : SettingsReader {
        int32_t getSetting(const char *settingName, int32_t defaultValue, DebugVarPrefix &type) override {
            type = DebugVarPrefix::none;
            if (strcmp(settingName, "EnableLEO") == 0) {
                return 3;
            }
            if (const char *envValue = IoFunctions::getenvPtr(settingName)) {
                return atoi(envValue);
            }
            return defaultValue;
        }
        int32_t getSetting(const char *settingName, int32_t defaultValue) override { return defaultValue; }
        int64_t getSetting(const char *settingName, int64_t defaultValue, DebugVarPrefix &type) override {
            type = DebugVarPrefix::none;
            return defaultValue;
        }
        int64_t getSetting(const char *settingName, int64_t defaultValue) override { return defaultValue; }
        bool getSetting(const char *settingName, bool defaultValue, DebugVarPrefix &type) override {
            type = DebugVarPrefix::none;
            return defaultValue;
        }
        bool getSetting(const char *settingName, bool defaultValue) override { return defaultValue; }
        std::string getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) override {
            type = DebugVarPrefix::none;
            if (strcmp(settingName, "ZE_AFFINITY_MASK") == 0) {
                return "0.9";
            }
            return value;
        }
        std::string getSetting(const char *settingName, const std::string &value) override { return value; }
        const char *appSpecificLocation(const std::string &name) override { return name.c_str(); }
    };

    for (bool fileConfigPresent : {true, false}) {
        // Must be set before FullyEnabledTestDebugManager is constructed - SettingsReaderCreator::
        // create() picks it up as readerImpl right there, during construction.
        VariableBackup<decltype(mockSettingsReader)> backupReader(&mockSettingsReader, {});
        VariableBackup<ApiSpecificConfig::ApiType> apiBackup(&apiTypeForUlts, ApiSpecificConfig::OCL);

        // The real OS environment always holds a *different* value for EnableLEO than readerImpl
        // does, plus values for a second regular variable readerImpl doesn't have, and for the raw
        // env variable.
        std::unordered_map<std::string, std::string> mockableEnvs = {
            {"EnableLEO", "2"},
            {"OverrideDefaultFP64Settings", "5"},
            {"NEO_CACHE_PERSISTENT", "42"},
            {"ZE_AFFINITY_MASK", "0.1"},
        };
        VariableBackup<decltype(IoFunctions::mockableEnvValues)> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        if (fileConfigPresent) {
            // readerImpl becomes the settings file - it replaces (rather than merges with) the
            // registry for regular debug/release variables.
            mockSettingsReader = std::make_unique<MockSettingFileReader>();
        } else {
            // No settings file - readerImpl falls back to the registry.
            mockSettingsReader = std::make_unique<MockRegistryReader>();
        }
        FullyEnabledTestDebugManager debugManager;

        if (fileConfigPresent) {
            EXPECT_EQ(1, debugManager.flags.EnableLEO.get());
            // readerImpl has no entry for this one and, unlike the registry, never falls back to the
            // environment - it stays at its default.
            EXPECT_EQ(-1, debugManager.flags.OverrideDefaultFP64Settings.get());
        } else {
            EXPECT_EQ(3, debugManager.flags.EnableLEO.get());
            // Not present "in the registry", so the registry reader's own env fallback kicks in.
            EXPECT_EQ(5, debugManager.flags.OverrideDefaultFP64Settings.get());
        }

        // Raw env variables are read through their own dedicated EnvironmentVariableReader,
        // completely independent of readerImpl - always the real environment, in both branches above.
        EXPECT_EQ(42, debugManager.flags.EnvCachePersistent.get());

        // Env-first release variables also always come from the real environment - winning over both
        // the settings file (which has its own, different value) and the registry (whose own mock
        // returns yet another value), in both branches above.
        EXPECT_STREQ("0.1", debugManager.flags.ZE_AFFINITY_MASK.get().c_str());
    }
}

} // namespace NEO
