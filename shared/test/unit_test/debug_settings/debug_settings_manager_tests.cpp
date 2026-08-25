/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/definitions/translate_debug_settings.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/flush_caches_bitmask.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/utilities/debug_file_reader.h"
#include "shared/source/utilities/logger.h"
#include "shared/test/common/debug_settings/debug_settings_manager_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_settings_reader.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include <memory>
#include <regex>
#include <sstream>
#include <string>

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
} // namespace NEO

template <typename VariableT, typename ValueT>
concept PubliclyMutableDebugVariable = requires(VariableT &variable, ValueT value) {
    variable.set(value);
};

TEST(DebugVariables, givenCompileTimeVariablesWhenCheckingTheirClassificationThenOnlyRuntimeAndReleaseVariablesAreMutable) {
    NEO::DebugVariablesT<true> debugVariables;

    static_assert(debugVariables.useCompileTimeVariables);

#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    static_assert(!PubliclyMutableDebugVariable<decltype(debugVariables.variableName), dataType>);
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, scope, description) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RUNTIME_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) \
    static_assert(PubliclyMutableDebugVariable<decltype(debugVariables.variableName), dataType>);
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_RUNTIME_DEBUG_VARIABLE
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE

#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) \
    static_assert(PubliclyMutableDebugVariable<decltype(debugVariables.variableName), dataType>);
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) \
    DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_ENV_FIRST(dataType, variableName, defaultValue, description) \
    DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_ENV_FIRST_OPT(enabled, dataType, variableName, defaultValue, description) \
    DECLARE_RELEASE_VARIABLE_ENV_FIRST(dataType, variableName, defaultValue, description)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_ENV_FIRST_OPT
#undef DECLARE_RELEASE_VARIABLE_ENV_FIRST
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE

    using CompileTimeVariable = decltype(debugVariables.EnableSWTags);
    static_assert(!CompileTimeVariable::get());

    EXPECT_FALSE(debugVariables.EnableSWTags.getRef());

    EXPECT_EQ("unk", debugVariables.OverrideGdiPath.get());
    debugVariables.OverrideGdiPath.set("test_gdi");
    EXPECT_EQ("test_gdi", debugVariables.OverrideGdiPath.get());

    EXPECT_EQ(-1, debugVariables.L2ClosNumCacheWays.get());

    EXPECT_EQ(-1, debugVariables.EnableLEO.get());
    debugVariables.EnableLEO.set(1);
    EXPECT_EQ(1, debugVariables.EnableLEO.get());
}

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
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_ENV_FIRST(dataType, variableName, defaultValue, description) DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_ENV_FIRST_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_RELEASE_VARIABLE_ENV_FIRST(dataType, variableName, defaultValue, description)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_ENV_FIRST_OPT
#undef DECLARE_RELEASE_VARIABLE_ENV_FIRST
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
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
    StreamCapture capture;
    capture.captureStdout();
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
    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    MockSettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE

    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWhenReleaseAndEnvVariablesAreNonDefaultThenDumpFlagsWritesThem) {
    StreamCapture capture;
    capture.captureStdout();
    FullyEnabledTestDebugManager debugManager;

    VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.PrintDebugSettings.setPrefixType(DebugVarPrefix::none);

    // Release variable example.
    debugManager.flags.EnableLEO.set(1);

    // Env variable example whose C++ member name differs from its real, external env name -
    // the dump must use the real name ("HOME"), never the translated member name ("EnvHome").
    debugManager.flags.EnvHome.set("/custom/home");

    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    MockSettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
    DebugVarPrefix type;
    EXPECT_EQ(1, allSettingsReader.getSetting("EnableLEO", -1, type));
    EXPECT_STREQ("/custom/home", allSettingsReader.getSetting("HOME", std::string("")).c_str());

    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: EnableLEO = 1"));
    EXPECT_EQ(std::string::npos, output.find("EnableLEO = 1 (env-only variable)"));

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: HOME = /custom/home (env-only variable)"));
    EXPECT_EQ(std::string::npos, output.find("EnvHome"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWithNeoPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    StreamCapture capture;
    capture.captureStdout();
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
    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    MockSettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE

    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWithLevelZeroPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    StreamCapture capture;
    capture.captureStdout();
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
    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    MockSettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE

    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWithOclPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    StreamCapture capture;
    capture.captureStdout();
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
    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    MockSettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE

    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_OCL_TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_OCL_LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_OCL_PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_OCL_Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWithMixedPrefixWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    StreamCapture capture;
    capture.captureStdout();
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
    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();

    // Validate allSettingsDumpFile
    MockSettingsFileReader allSettingsReader{FullyEnabledTestDebugManager::settingsDumpFileName};
#define DECLARE_DEBUG_VARIABLE(dataType, varName, defaultValue, description)                                     \
    {                                                                                                            \
        DebugVarPrefix type;                                                                                     \
        EXPECT_EQ(debugManager.flags.varName.get(), allSettingsReader.getSetting(#varName, defaultValue, type)); \
    }
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE

    removeVirtualFile(FullyEnabledTestDebugManager::settingsDumpFileName);
    std::string output = capture.getCapturedStdout();
    ASSERT_NE(0u, output.size());

    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_TbxServer = 192.168.0.1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_LoopAtDriverInit = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: NEO_L0_PrintDebugSettings = 1"));
    EXPECT_NE(std::string::npos, output.find("Non-default value of debug variable: Enable64kbpages = 1"));
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledOnDisabledDebugManagerWhenCallingDumpFlagsThenFlagsAreNotWrittenToDumpFile) {
    StreamCapture capture;
    capture.captureStdout();
    FullyDisabledTestDebugManager debugManager;
    debugManager.flags.PrintDebugSettings.set(true);

    removeVirtualFile(FullyDisabledTestDebugManager::settingsDumpFileName);
    debugManager.dumpFlags();
    removeVirtualFile(FullyDisabledTestDebugManager::settingsDumpFileName);

    std::string output = capture.getCapturedStdout();
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
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));
    constexpr std::string_view data = "LogApiCalls = 1\nEnableLEO=1";
    NEO::writeDataToFile(SettingsReader::settingsFileName, data, false);

    SettingsReader *reader = MockSettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    FullyDisabledTestDebugManager debugManager;
    debugManager.setReaderImpl(reader);
    debugManager.injectSettingsFromReader();
    EXPECT_EQ(1, debugManager.flags.EnableLEO.get());
    EXPECT_EQ(0, debugManager.flags.LogApiCalls.get());

    removeVirtualFile(SettingsReader::settingsFileName);
}

TEST(DebugSettingsManager, givenEnabledDebugManagerWhenCreateThenAllVariablesAreRead) {
    constexpr std::string_view data = "LogApiCalls = 1\nMakeAllBuffersResident = 1";
    NEO::writeDataToFile(SettingsReader::settingsFileName, data, false);

    SettingsReader *reader = MockSettingsReader::createFileReader();
    EXPECT_NE(nullptr, reader);

    FullyEnabledTestDebugManager debugManager;
    debugManager.setReaderImpl(reader);
    debugManager.injectSettingsFromReader();

    EXPECT_EQ(1, debugManager.flags.MakeAllBuffersResident.get());
    EXPECT_EQ(1, debugManager.flags.LogApiCalls.get());

    removeVirtualFile(SettingsReader::settingsFileName);
}

TEST(DebugSettingsManager, GivenLogsEnabledAndDumpToFileWhenPrintDebuggerLogCalledThenStringPrintedToFile) {
    if (!NEO::fileLoggerInstance().enabled()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::DUMP_TO_FILE);

    auto logFile = NEO::fileLoggerInstance().getLogFileName();

    removeVirtualFile(logFile);

    StreamCapture capture;
    capture.captureStdout();
    PRINT_DEBUGGER_LOG(stdout, "test %s", "log");
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());

    ASSERT_TRUE(virtualFileExists(logFile));

    size_t retSize;
    auto data = loadDataFromVirtualFile(logFile, retSize);

    EXPECT_STREQ("test log", data.get());
    removeVirtualFile(logFile);
}

TEST(DebugSettingsManager, GivenLogsDisabledAndDumpToFileWhenPrintDebuggerLogCalledThenStringIsNotPrintedToFile) {
    if (!NEO::debugManager.disabled()) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebuggerLogBitmask.set(NEO::DebugVariables::DEBUGGER_LOG_BITMASK::DUMP_TO_FILE);

    auto logFile = NEO::fileLoggerInstance().getLogFileName();
    removeVirtualFile(logFile);

    StreamCapture capture;
    capture.captureStdout();
    PRINT_DEBUGGER_LOG(stdout, "test %s", "log");

    auto output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());

    ASSERT_FALSE(virtualFileExists(logFile));
}

TEST(DebugSettingsManager, GivenLogsEnabledWhenLogCacheOperationCalledThenStringPrintedToFile) {
    DebugManagerStateRestore restorer;
    MockExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(executionEnvironment);
    auto &logger = executionEnvironment.getUsmReusePerfLogger();
    if (!logger.enabled()) {
        GTEST_SKIP();
    }

    auto logFile = logger.getLogFileName();
    removeVirtualFile(logFile);

    SVMAllocsManager::SvmAllocationCache svmAllocationCache;
    svmAllocationCache.memoryManager = &memoryManager;
    auto timePoint = std::chrono::high_resolution_clock::now();
    svmAllocationCache.logCacheOperation({.allocationSize = 1024,
                                          .timePoint = timePoint,
                                          .allocationType = InternalMemoryType::deviceUnifiedMemory,
                                          .operationType = SVMAllocsManager::SvmAllocationCache::CacheOperationType::trim,
                                          .isSuccess = true});

    auto logFileExists = NEO::fileExists(logFile);
    EXPECT_TRUE(logFileExists);

    size_t retSize;
    auto data = loadDataFromVirtualFile(logFile, retSize);
    auto retString = std::string(data.get(), data.get() + retSize);
    std::stringstream expectedString;
    expectedString << timePoint.time_since_epoch().count() << " , "
                   << "device"
                   << " , "
                   << "trim"
                   << " , "
                   << "1024"
                   << " , "
                   << "TRUE";

    EXPECT_NE(std::string::npos, retString.find(expectedString.str()));
    removeVirtualFile(logFile);
}

TEST(DebugLog, WhenLogDebugStringCalledThenNothingIsPrintedToStdout) {
    StreamCapture capture;
    capture.captureStdout();
    logDebugString("test log");
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());
}

TEST(DurationLogTest, givenDurationGetTimeStringThenTimeStringIsCorrect) {
    auto timeString = DurationLog::getTimeString();
    for (auto c : timeString) {
        EXPECT_TRUE(std::isdigit(c) || c == '[' || c == ']' || c == '.' || c == ' ');
    }
}

TEST(DebugSettingsManager, GivenTbxOrTbxWithAubCsrTypeAndTbxFaultsEnabledWhenCallingIsTbxMngrEnabledThenReturnTrue) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableTbxPageFaultManager.set(1);

    NEO::debugManager.flags.SetCommandStreamReceiver.set(2);
    EXPECT_TRUE(NEO::debugManager.isTbxPageFaultManagerEnabled());

    NEO::debugManager.flags.SetCommandStreamReceiver.set(4);
    EXPECT_TRUE(NEO::debugManager.isTbxPageFaultManagerEnabled());
}

TEST(DebugSettingsManager, GivenTbxOrTbxWithAubCsrTypeAndAllElseDefaultWhenCallingIsTbxMngrEnabledThenReturnTrue) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.SetCommandStreamReceiver.set(2);
    EXPECT_TRUE(NEO::debugManager.isTbxPageFaultManagerEnabled());

    NEO::debugManager.flags.SetCommandStreamReceiver.set(4);
    EXPECT_TRUE(NEO::debugManager.isTbxPageFaultManagerEnabled());
}

TEST(DebugSettingsManager, givenFlushAllCachesWhenTranslateDebugSettingsThenOverrideEnv) {
    DebugManagerStateRestore restorer;
    {
        NEO::debugManager.flags.FlushAllCaches.set(1);
        translateDebugSettings(NEO::debugManager.flags);
        EXPECT_EQ(NEO::debugManager.flags.FlushAllCaches.get(), FlushCachesBitmask::allCaches);
    }

    {
        NEO::debugManager.flags.FlushAllCaches.set(1 | FlushCachesBitmask::dcFlush);
        translateDebugSettings(NEO::debugManager.flags);
        EXPECT_EQ(NEO::debugManager.flags.FlushAllCaches.get(), FlushCachesBitmask::allCaches);
    }

    {
        NEO::debugManager.flags.FlushAllCaches.set(0);
        translateDebugSettings(NEO::debugManager.flags);
        EXPECT_EQ(NEO::debugManager.flags.FlushAllCaches.get(), 0);
    }

    {
        NEO::debugManager.flags.FlushAllCaches.set(FlushCachesBitmask::constantCache);
        translateDebugSettings(NEO::debugManager.flags);
        EXPECT_EQ(NEO::debugManager.flags.FlushAllCaches.get(), FlushCachesBitmask::constantCache);
    }
}

TEST(DebugSettingsManager, GivenTbxFaultsDisabledWhenCallingIsTbxMngrEnabledThenReturnFalse) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableTbxPageFaultManager.set(0);

    EXPECT_FALSE(NEO::debugManager.isTbxPageFaultManagerEnabled());
}

TEST(DebugSettingsManager, GivenHardwareOrHardwareWithAubCsrTypeAndTbxFaultsEnabledWhenCallingIsTbxMngrEnabledThenReturnFalse) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableTbxPageFaultManager.set(1);

    NEO::debugManager.flags.SetCommandStreamReceiver.set(1);
    EXPECT_FALSE(NEO::debugManager.isTbxPageFaultManagerEnabled());

    NEO::debugManager.flags.SetCommandStreamReceiver.set(3);
    EXPECT_FALSE(NEO::debugManager.isTbxPageFaultManagerEnabled());
}

TEST(DebugSettingsManager, whenDebugVariableDoesntMatchScopeThenIgnoreIt) {
    auto defaultValue = debugManager.flags.TbxPort.get();
    struct MockSettingFileReader : SettingsFileReader {
        MockSettingFileReader() : SettingsFileReader("") {
            settingStringMap["TbxPort"] = "1";
            settingStringMap["NEO_TbxPort"] = "1";
            settingStringMap["NEO_OCL_TbxPort"] = "2";
            settingStringMap["NEO_L0_TbxPort"] = "3";
        }
    };

    VariableBackup<decltype(mockSettingsReader)> backupReader(&mockSettingsReader, {});
    VariableBackup backupPrefixes(&validUltPrefixTypesOverride);

    // EnvCachePersistent is a raw env variable - it is always read from the real OS environment,
    // never from mockSettingsReader/a settings file, so it must be mocked separately here.
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_CACHE_PERSISTENT", "1"}};
    VariableBackup<decltype(IoFunctions::mockableEnvValues)> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    {
        mockSettingsReader = std::make_unique<MockSettingFileReader>();
        FullyEnabledTestDebugManager debugManager;
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_EQ(2, debugManager.flags.TbxPort.get());
        EXPECT_EQ(1, debugManager.flags.EnvCachePersistent.get());
    }

    {
        mockSettingsReader = std::make_unique<MockSettingFileReader>();
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        FullyEnabledTestDebugManager debugManager;
        EXPECT_EQ(3, debugManager.flags.TbxPort.get());
        EXPECT_EQ(1, debugManager.flags.EnvCachePersistent.get());
    }

    {
        mockSettingsReader = std::make_unique<MockSettingFileReader>();
        StackVec<DebugVarPrefix, 4> prefixes = {};
        validUltPrefixTypesOverride = &prefixes;
        FullyEnabledTestDebugManager debugManager;
        EXPECT_EQ(defaultValue, debugManager.flags.TbxPort.get());
        EXPECT_EQ(-1, debugManager.flags.EnvCachePersistent.get());
    }
}

TEST(DebugSettingsManager, givenFileOrEnvironmentReaderActiveThenRegularVariablesFollowItWhileRawEnvVariablesAlwaysComeFromRealEnvironment) {
    // Plays the role of a neo.config/igdrcl.config settings file. Deliberately has no entry for
    // OverrideDefaultFP64Settings - unlike RegistryReader, SettingsFileReader has no fallback to the
    // environment for keys it doesn't contain.
    struct MockSettingFileReader : SettingsFileReader {
        MockSettingFileReader() : SettingsFileReader("") {
            settingStringMap["EnableLEO"] = "1";
        }
    };

    for (bool fileConfigPresent : {true, false}) {
        // Must be set before FullyEnabledTestDebugManager is constructed - SettingsReaderCreator::
        // create() picks it up as readerImpl right there, during construction.
        VariableBackup<decltype(mockSettingsReader)> backupReader(&mockSettingsReader, {});
        VariableBackup<ApiSpecificConfig::ApiType> apiBackup(&apiTypeForUlts, ApiSpecificConfig::OCL);

        // The real OS environment always holds a *different* value for EnableLEO than the file does,
        // plus values for a second regular variable the file doesn't have, and for the raw env variable.
        std::unordered_map<std::string, std::string> mockableEnvs = {
            {"EnableLEO", "2"},
            {"OverrideDefaultFP64Settings", "5"},
            {"NEO_CACHE_PERSISTENT", "42"},
        };
        VariableBackup<decltype(IoFunctions::mockableEnvValues)> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        if (fileConfigPresent) {
            // readerImpl becomes the settings file - it replaces (rather than merges with) the
            // OS-native reader for regular debug/release variables.
            mockSettingsReader = std::make_unique<MockSettingFileReader>();
        } else {
            // No settings file - readerImpl falls back to the OS-native reader
            mockSettingsReader = std::make_unique<EnvironmentVariableReader>();
        }
        FullyEnabledTestDebugManager debugManager;

        if (fileConfigPresent) {
            EXPECT_EQ(1, debugManager.flags.EnableLEO.get());
            // The file has no entry for this one and, unlike the registry, never falls back to the
            // environment - it stays at its default.
            EXPECT_EQ(-1, debugManager.flags.OverrideDefaultFP64Settings.get());
        } else {
            EXPECT_EQ(2, debugManager.flags.EnableLEO.get());
            EXPECT_EQ(5, debugManager.flags.OverrideDefaultFP64Settings.get());
        }

        // Raw env variables are read through their own dedicated EnvironmentVariableReader,
        // completely independent of readerImpl - always the real environment, in both branches above.
        EXPECT_EQ(42, debugManager.flags.EnvCachePersistent.get());
    }
}

TEST(DebugSettingsManager, givenEnvFirstReleaseVariableWhenRealEnvironmentHasItThenItWinsOverAFilePresentSettingsFileOtherwiseTheFileWins) {
    // Plays the role of a neo.config/igdrcl.config settings file that explicitly sets
    // ZE_AFFINITY_MASK - an env-first release variable (Level Zero spec name).
    struct MockSettingFileReader : SettingsFileReader {
        MockSettingFileReader() : SettingsFileReader("") {
            settingStringMap["ZE_AFFINITY_MASK"] = "0.5";
        }
    };

    for (bool envValuePresent : {true, false}) {
        VariableBackup<decltype(mockSettingsReader)> backupReader(&mockSettingsReader, {});
        VariableBackup<ApiSpecificConfig::ApiType> apiBackup(&apiTypeForUlts, ApiSpecificConfig::OCL);

        std::unordered_map<std::string, std::string> mockableEnvs;
        if (envValuePresent) {
            mockableEnvs["ZE_AFFINITY_MASK"] = "0.1";
        }
        VariableBackup<decltype(IoFunctions::mockableEnvValues)> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        // The settings file is present in both iterations - it must not matter for an env-first variable.
        mockSettingsReader = std::make_unique<MockSettingFileReader>();
        FullyEnabledTestDebugManager debugManager;

        if (envValuePresent) {
            EXPECT_STREQ("0.1", debugManager.flags.ZE_AFFINITY_MASK.get().c_str());
        } else {
            EXPECT_STREQ("0.5", debugManager.flags.ZE_AFFINITY_MASK.get().c_str());
        }
    }
}
