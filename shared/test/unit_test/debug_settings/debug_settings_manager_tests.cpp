/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/debug_file_reader.h"
#include "shared/source/utilities/logger.h"
#include "shared/test/common/debug_settings/debug_settings_manager_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} // namespace NEO

TEST(DebugSettingsManager, WhenDebugManagerIsCreatedThenInjectFcnIsNull) {
    FullyEnabledTestDebugManager debugManager;

    EXPECT_FALSE(debugManager.disabled());

    EXPECT_EQ(nullptr, debugManager.injectFcn);
}

TEST(DebugSettingsManager, WhenDebugManagerIsCreatedThenSettingsReaderIsAvailable) {
    FullyEnabledTestDebugManager debugManager;
    // SettingsReader created
    EXPECT_NE(nullptr, debugManager.getSettingsReader());
}

TEST(DebugSettingsManager, WhenDebugManagerIsDisabledThenDebugFunctionalityIsNotAvailable) {
    FullyDisabledTestDebugManager debugManager;

    // Should not be enabled without debug functionality
    EXPECT_TRUE(debugManager.disabled());

// debug variables / flags set to default
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)                                                  \
    {                                                                                                                              \
        bool isEqual = TestDebugFlagsChecker::isEqual(debugManager.flags.variableName.get(), static_cast<dataType>(defaultValue)); \
        EXPECT_TRUE(isEqual);                                                                                                      \
    }
#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE
}

TEST(DebugSettingsManager, whenDebugManagerIsDisabledThenDebugFunctionalityIsNotAvailableAtCompileTime) {
    TestDebugSettingsManager<DebugFunctionalityLevel::none> debugManager;

    static_assert(debugManager.disabled(), "");
    static_assert(false == debugManager.registryReadAvailable(), "");
}

TEST(DebugSettingsManager, whenDebugManagerIsFullyEnabledThenAllDebugFunctionalityIsAvailableAtCompileTime) {
    TestDebugSettingsManager<DebugFunctionalityLevel::full> debugManager;

    static_assert(false == debugManager.disabled(), "");
    static_assert(debugManager.registryReadAvailable(), "");
}

TEST(DebugSettingsManager, whenOnlyRegKeysAreEnabledThenAllOtherDebugFunctionalityIsNotAvailableAtCompileTime) {
    TestDebugSettingsManager<DebugFunctionalityLevel::regKeys> debugManager;

    static_assert(false == debugManager.disabled(), "");
    static_assert(debugManager.registryReadAvailable(), "");
}

TEST(DebugSettingsManager, givenTwoPossibleVariantsOfHardwareInfoOverrideStringThenOutputStringIsTheSame) {
    FullyEnabledTestDebugManager debugManager;
    std::string hwInfoConfig;

    // Set HardwareInfoOverride as regular string (i.e. as in Windows Registry)
    std::string str1 = "1x4x8";
    debugManager.flags.HardwareInfoOverride.set(str1);
    debugManager.getHardwareInfoOverride(hwInfoConfig);
    EXPECT_EQ(str1, hwInfoConfig);

    // Set HardwareInfoOverride as quoted string (i.e. as in igdrcl.config file)
    std::string str2 = "\"1x4x8\"";
    debugManager.flags.HardwareInfoOverride.set(str2);
    hwInfoConfig = debugManager.flags.HardwareInfoOverride.get();
    EXPECT_EQ(str2, hwInfoConfig);
    debugManager.getHardwareInfoOverride(hwInfoConfig);
    EXPECT_EQ(str1, hwInfoConfig);
}

TEST(DebugSettingsManager, givenStringDebugVariableWhenLongValueExeedingSmallStringOptimizationIsAssignedThenMemoryLeakIsNotReported) {
    DebugManagerStateRestore debugManagerStateRestore;
    debugManager.flags.AUBDumpCaptureFileName.set("ThisIsVeryLongStringValueThatExceedSizeSpecifiedBySmallStringOptimizationAndCausesInternalStringBufferResize");
}

TEST(DebugSettingsManager, givenNullAsReaderImplInDebugManagerWhenSettingReaderImplThenItsSetProperly) {
    FullyDisabledTestDebugManager debugManager;
    auto readerImpl = SettingsReader::create("");
    debugManager.setReaderImpl(readerImpl);
    EXPECT_EQ(readerImpl, debugManager.getReaderImpl());
}
TEST(DebugSettingsManager, givenReaderImplInDebugManagerWhenSettingDifferentReaderImplThenItsSetProperly) {
    FullyDisabledTestDebugManager debugManager;
    auto readerImpl = SettingsReader::create("");
    debugManager.setReaderImpl(readerImpl);

    auto readerImpl2 = SettingsReader::create("");
    debugManager.setReaderImpl(readerImpl2);
    EXPECT_EQ(readerImpl2, debugManager.getReaderImpl());
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWithNoPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    testing::internal::CaptureStdout();
    FullyEnabledTestDebugManager debugManager;

    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.PrintDebugSettings.setPrefixType(DebugVarPrefix::none);
    debugManager.flags.LoopAtDriverInit.set(true);
    debugManager.flags.LoopAtDriverInit.setPrefixType(DebugVarPrefix::none);
    debugManager.flags.Enable64kbpages.set(1);
    debugManager.flags.Enable64kbpages.setPrefixType(DebugVarPrefix::none);
    debugManager.flags.TbxServer.set("192.168.0.1");
    debugManager.flags.TbxServer.setPrefixType(DebugVarPrefix::none);

    // Clear dump files and generate new
    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    SettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }

#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE

    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = testing::internal::GetCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, DISABLED_givenPrintDebugSettingsEnabledWithNeoPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    testing::internal::CaptureStdout();
    FullyEnabledTestDebugManager debugManager;

    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.PrintDebugSettings.setPrefixType(DebugVarPrefix::neo);
    debugManager.flags.LoopAtDriverInit.set(true);
    debugManager.flags.LoopAtDriverInit.setPrefixType(DebugVarPrefix::neo);
    debugManager.flags.Enable64kbpages.set(1);
    debugManager.flags.Enable64kbpages.setPrefixType(DebugVarPrefix::neo);
    debugManager.flags.TbxServer.set("192.168.0.1");
    debugManager.flags.TbxServer.setPrefixType(DebugVarPrefix::neo);

    // Clear dump files and generate new
    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    SettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }

#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE

    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = testing::internal::GetCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWithLevelZeroPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    testing::internal::CaptureStdout();
    FullyEnabledTestDebugManager debugManager;

    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.PrintDebugSettings.setPrefixType(DebugVarPrefix::neoL0);
    debugManager.flags.LoopAtDriverInit.set(true);
    debugManager.flags.LoopAtDriverInit.setPrefixType(DebugVarPrefix::neoL0);
    debugManager.flags.Enable64kbpages.set(1);
    debugManager.flags.Enable64kbpages.setPrefixType(DebugVarPrefix::neoL0);
    debugManager.flags.TbxServer.set("192.168.0.1");
    debugManager.flags.TbxServer.setPrefixType(DebugVarPrefix::neoL0);

    // Clear dump files and generate new
    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    SettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }

#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE

    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = testing::internal::GetCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWithOclPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    testing::internal::CaptureStdout();
    FullyEnabledTestDebugManager debugManager;

    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.PrintDebugSettings.setPrefixType(DebugVarPrefix::neoOcl);
    debugManager.flags.LoopAtDriverInit.set(true);
    debugManager.flags.LoopAtDriverInit.setPrefixType(DebugVarPrefix::neoOcl);
    debugManager.flags.Enable64kbpages.set(1);
    debugManager.flags.Enable64kbpages.setPrefixType(DebugVarPrefix::neoOcl);
    debugManager.flags.TbxServer.set("192.168.0.1");
    debugManager.flags.TbxServer.setPrefixType(DebugVarPrefix::neoOcl);

    // Clear dump files and generate new
    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    SettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }

#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE

    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = testing::internal::GetCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_OCL_TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_OCL_LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_OCL_PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_OCL_Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWithMixedPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    testing::internal::CaptureStdout();
    FullyEnabledTestDebugManager debugManager;

    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.PrintDebugSettings.setPrefixType(DebugVarPrefix::neoL0);
    debugManager.flags.LoopAtDriverInit.set(true);
    debugManager.flags.LoopAtDriverInit.setPrefixType(DebugVarPrefix::neo);
    debugManager.flags.Enable64kbpages.set(1);
    debugManager.flags.Enable64kbpages.setPrefixType(DebugVarPrefix::none);
    debugManager.flags.TbxServer.set("192.168.0.1");
    debugManager.flags.TbxServer.setPrefixType(DebugVarPrefix::neoL0);

    // Clear dump files and generate new
    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    SettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }

#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE

    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = testing::internal::GetCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledOnDisabledDebugManagerWhenCallingDumpFlagsThenFlagsAreNotWrittenToDumpFile) {
    testing::internal::CaptureStdout();
    FullyDisabledTestDebugManager debugManager;
    debugManager.flags.PrintDebugSettings.set(true);

    std::remove(FullyDisabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();
    std::remove(FullyDisabledTestDebugManager::settingsDumpFileName);

    std::string output = testing::internal::GetCapturedStdout();
    ASSERT_EQ(0u, output.size());
}

TEST(AllocationInfoLogging, givenBaseGraphicsAllocationWhenGettingImplementationSpecificAllocationInfoThenReturnEmptyInfoString) {
    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);
    EXPECT_STREQ(graphicsAllocation.getAllocationInfoString().c_str(), "");
}

TEST(AllocationInfoLogging, givenBaseGraphicsAllocationWhenGettingImplementationSpecificPatIndexInfoThenReturnEmptyInfoString) {
    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);

    MockProductHelper productHelper{};
    EXPECT_STREQ(graphicsAllocation.getPatIndexInfoString(productHelper).c_str(), "");
}

TEST(DebugSettingsManager, givenDisabledDebugManagerWhenCreateThenOnlyReleaseVariablesAreRead) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        const char data[] = "LogApiCalls = 1\nMakeAllBuffersResident = 1";
        std::ofstream file;
        file.open(SettingsReader::settingsFileName);
        file << data;
        file.close();
    }

    SettingsReader *reader = SettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    FullyDisabledTestDebugManager debugManager;
    debugManager.setReaderImpl(reader);
    debugManager.injectSettingsFromReader();

    EXPECT_EQ(1, debugManager.flags.MakeAllBuffersResident.get());
    EXPECT_EQ(0, debugManager.flags.LogApiCalls.get());

    if (!settingsFileExists) {
        std::remove(SettingsReader::settingsFileName);
    }
}

TEST(DebugSettingsManager, givenEnabledDebugManagerWhenCreateThenAllVariablesAreRead) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        const char data[] = "LogApiCalls = 1\nMakeAllBuffersResident = 1";
        std::ofstream file;
        file.open(SettingsReader::settingsFileName);
        file << data;
        file.close();
    }

    SettingsReader *reader = SettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    FullyEnabledTestDebugManager debugManager;
    debugManager.setReaderImpl(reader);
    debugManager.injectSettingsFromReader();

    EXPECT_EQ(1, debugManager.flags.MakeAllBuffersResident.get());
    EXPECT_EQ(1, debugManager.flags.LogApiCalls.get());

    if (!settingsFileExists) {
        std::remove(SettingsReader::settingsFileName);
    }
}

TEST(DebugSettingsManager, GivenLogsEnabledAndDumpToFileWhenPrintDebuggerLogCalledThenStringPrintedToFile) {
    if (!NEO::fileLoggerInstance().enabled()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::DUMP_TO_FILE);

    auto logFile = NEO::fileLoggerInstance().getLogFileName();

    std::remove(logFile);

    ::testing::internal::CaptureStdout();
    PRINT_DEBUGGER_LOG(stdout, "test %s", "log");
    auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_EQ(0u, output.size());

    auto logFileExists = fileExists(logFile);
    EXPECT_TRUE(logFileExists);

    size_t retSize;
    auto data = loadDataFromFile(logFile, retSize);

    EXPECT_STREQ("test log", data.get());
    std::remove(logFile);
}

TEST(DebugSettingsManager, GivenLogsDisabledAndDumpToFileWhenPrintDebuggerLogCalledThenStringIsNotPrintedToFile) {
    if (!NEO::debugManager.disabled()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::DUMP_TO_FILE);

    auto logFile = NEO::fileLoggerInstance().getLogFileName();
    std::remove(logFile);

    ::testing::internal::CaptureStdout();
    PRINT_DEBUGGER_LOG(stdout, "test %s", "log");

    auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_EQ(0u, output.size());

    auto logFileExists = fileExists(logFile);
    ASSERT_FALSE(logFileExists);
}

TEST(DebugLog, WhenLogDebugStringCalledThenNothingIsPrintedToStdout) {
    ::testing::internal::CaptureStdout();
    logDebugString("test log");
    auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_EQ(0u, output.size());
}

TEST(DurationLogTest, givenDurationGetTimeStringThenTimeStringIsCorrect) {
    auto timeString = DurationLog::getTimeString();
    for (auto c : timeString) {
        EXPECT_TRUE(std::isdigit(c) || c == '[' || c == ']' || c == '.' || c == ' ');
    }
}
