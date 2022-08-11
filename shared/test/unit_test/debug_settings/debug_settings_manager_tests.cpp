/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/debug_file_reader.h"
#include "shared/test/common/debug_settings/debug_settings_manager_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>

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
    TestDebugSettingsManager<DebugFunctionalityLevel::None> debugManager;

    static_assert(debugManager.disabled(), "");
    static_assert(false == debugManager.registryReadAvailable(), "");
}

TEST(DebugSettingsManager, whenDebugManagerIsFullyEnabledThenAllDebugFunctionalityIsAvailableAtCompileTime) {
    TestDebugSettingsManager<DebugFunctionalityLevel::Full> debugManager;

    static_assert(false == debugManager.disabled(), "");
    static_assert(debugManager.registryReadAvailable(), "");
}

TEST(DebugSettingsManager, whenOnlyRegKeysAreEnabledThenAllOtherDebugFunctionalityIsNotAvailableAtCompileTime) {
    TestDebugSettingsManager<DebugFunctionalityLevel::RegKeys> debugManager;

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
    DebugManager.flags.AUBDumpCaptureFileName.set("ThisIsVeryLongStringValueThatExceedSizeSpecifiedBySmallStringOptimizationAndCausesInternalStringBufferResize");
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

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    testing::internal::CaptureStdout();
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.LoopAtDriverInit.set(true);
    debugManager.flags.Enable64kbpages.set(1);
    debugManager.flags.TbxServer.set("192.168.0.1");

    // Clear dump files and generate new
    std::remove(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    SettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description) \
    EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue));

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
    GraphicsAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu);
    EXPECT_STREQ(graphicsAllocation.getAllocationInfoString().c_str(), "");
}

TEST(DebugSettingsManager, givenDisabledDebugManagerWhenCreateThenOnlyReleaseVariablesAreRead) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        const char data[] = "LogApiCalls = 1\nMakeAllBuffersResident = 1";
        writeDataToFile(SettingsReader::settingsFileName, &data, sizeof(data));
    }

    SettingsReader *reader = SettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    FullyDisabledTestDebugManager debugManager;
    debugManager.setReaderImpl(reader);
    debugManager.injectSettingsFromReader();

    EXPECT_EQ(1, debugManager.flags.MakeAllBuffersResident.get());
    EXPECT_EQ(0, debugManager.flags.LogApiCalls.get());

    if (!settingsFileExists) {
        remove(SettingsReader::settingsFileName);
    }
}

TEST(DebugSettingsManager, givenEnabledDebugManagerWhenCreateThenAllVariablesAreRead) {
    bool settingsFileExists = fileExists(SettingsReader::settingsFileName);
    if (!settingsFileExists) {
        const char data[] = "LogApiCalls = 1\nMakeAllBuffersResident = 1";
        writeDataToFile(SettingsReader::settingsFileName, &data, sizeof(data));
    }

    SettingsReader *reader = SettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    FullyEnabledTestDebugManager debugManager;
    debugManager.setReaderImpl(reader);
    debugManager.injectSettingsFromReader();

    EXPECT_EQ(1, debugManager.flags.MakeAllBuffersResident.get());
    EXPECT_EQ(1, debugManager.flags.LogApiCalls.get());

    if (!settingsFileExists) {
        remove(SettingsReader::settingsFileName);
    }
}
