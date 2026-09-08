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

TEST(DebugSettingsManager, givenFileOrRegistryReaderActiveThenReleaseVariablesFallBackToEnvironmentWhileDebugVariablesDoNot) {
    // Plays the role of a neo.config/igdrcl.config settings file. Deliberately has no entry for
    // OverrideDefaultFP64Settings (a release variable, so it falls back to the environment) nor for
    // LogApiCalls (a debug variable, so it does not).
    struct MockSettingFileReader : SettingsFileReader {
        MockSettingFileReader() : SettingsFileReader("") {
            settingStringMap["EnableLEO"] = "1";
        }
    };

    // Mirrors RegistryReader's own two-tier behavior (shared/source/os_interface/windows/
    // debug_registry_reader.cpp): a real registry hit for EnableLEO only; every other key falls
    // through to the same environment check RegistryReader itself performs when the registry misses.
    struct MockRegistryReader : SettingsReader {
        bool hasSetting(const char *settingName, DebugVarPrefix &type) override {
            type = DebugVarPrefix::none;
            if (strcmp(settingName, "EnableLEO") == 0) {
                return true;
            }
            return nullptr != IoFunctions::getenvPtr(settingName);
        }
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
            if (const char *envValue = IoFunctions::getenvPtr(settingName)) {
                return 0 != atoi(envValue);
            }
            return defaultValue;
        }
        bool getSetting(const char *settingName, bool defaultValue) override { return defaultValue; }
        std::string getSetting(const char *settingName, const std::string &value, DebugVarPrefix &type) override {
            type = DebugVarPrefix::none;
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
            {"LogApiCalls", "1"},
            {"NEO_CACHE_PERSISTENT", "42"},
        };
        VariableBackup<decltype(IoFunctions::mockableEnvValues)> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        if (fileConfigPresent) {
            // readerImpl becomes the settings file - authoritative for the keys it holds, with the
            // environment still consulted for release variables it doesn't.
            mockSettingsReader = std::make_unique<MockSettingFileReader>();
        } else {
            // No settings file - readerImpl falls back to the registry.
            mockSettingsReader = std::make_unique<MockRegistryReader>();
        }
        FullyEnabledTestDebugManager debugManager;

        if (fileConfigPresent) {
            EXPECT_EQ(1, debugManager.flags.EnableLEO.get());
        } else {
            EXPECT_EQ(3, debugManager.flags.EnableLEO.get());
        }

        // A release variable readerImpl doesn't hold falls back to the environment either way: via
        // the merge when readerImpl is the settings file, via RegistryReader's own env fallback
        // otherwise.
        EXPECT_EQ(5, debugManager.flags.OverrideDefaultFP64Settings.get());

        if (fileConfigPresent) {
            // Debug variables get no such fallback - an existing settings file suppresses registry
            // and environment reads for them, so this stays at its default despite being in the env.
            EXPECT_FALSE(debugManager.flags.LogApiCalls.get());
        } else {
            EXPECT_TRUE(debugManager.flags.LogApiCalls.get());
        }

        // Raw env variables are read through their own dedicated EnvironmentVariableReader,
        // completely independent of readerImpl - always the real environment, in both branches above.
        EXPECT_EQ(42, debugManager.flags.EnvCachePersistent.get());
    }
}

} // namespace NEO
