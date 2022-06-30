/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/utilities/debug_file_reader.h"
#include "shared/test/common/debug_settings/debug_settings_manager_fixture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include <unordered_map>

namespace NEO {

TEST(DebugSettingsManager, givenDisabledDebugManagerAndMockEnvVariableWhenCreateThenAllVariablesAreRead) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        const char data[] = "LogApiCalls = 1\nMakeAllBuffersResident = 1";
        writeDataToFile(SettingsReader::settingsFileName, &data, sizeof(data));
    }

    SettingsReader *reader = SettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEOReadDebugKeys", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    FullyDisabledTestDebugManager debugManager;
    debugManager.setReaderImpl(reader);
    debugManager.injectSettingsFromReader();

    EXPECT_EQ(1, debugManager.flags.MakeAllBuffersResident.get());
    EXPECT_EQ(1, debugManager.flags.LogApiCalls.get());

    if (!settingsFileExists) {
        remove(SettingsReader::settingsFileName);
    }
}

TEST(DebugSettingsManager, givenPrintDebugSettingsAndDebugKeysReadEnabledOnDisabledDebugManagerWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    testing::internal::CaptureStdout();
    FullyDisabledTestDebugManager debugManager;
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.LoopAtDriverInit.set(true);
    debugManager.flags.Enable64kbpages.set(1);
    debugManager.flags.TbxServer.set("192.168.0.1");

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEOReadDebugKeys", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    // Clear dump files and generate new
    std::remove(FullyDisabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    SettingsFileReader allSettingsReader{FullyDisabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description) \
    EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue));

#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
    std::remove(FullyDisabledTestDebugManager::settingsDumpFileName);
    std::string output = testing::internal::GetCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: Enable64kbpages = 1"));
}

} // namespace NEO
