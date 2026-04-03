/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_settings_reader.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <string>

using namespace NEO;

namespace SettingsReaderTests {

TEST(SettingsReader, WhenCreatingSettingsReaderThenReaderIsCreated) {
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
}

TEST(SettingsReader, GivenNoSettingsFileWhenCreatingSettingsReaderThenOsReaderIsCreated) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> fopenBackup(&NEO::IoFunctions::fopenPtr, [](const char *, const char *) -> FILE * {
        return nullptr;
    });

    auto fileReader = std::unique_ptr<SettingsReader>(SettingsReader::createFileReader());
    EXPECT_EQ(nullptr, fileReader.get());

    auto osReader = std::unique_ptr<SettingsReader>(SettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, osReader.get());
}

TEST(SettingsReader, GivenSettingsFileExistsWithZeroSizeWhenCreatingFileReaderThenReaderIsCreatedWithNoSettings) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));

    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> fopenBackup(&NEO::IoFunctions::fopenPtr, [](const char *, const char *) -> FILE * {
        return reinterpret_cast<FILE *>(1);
    });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> fseekBackup(&NEO::IoFunctions::fseekPtr, [](FILE *, long int, int) -> int {
        return 0;
    });
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> ftellBackup(&NEO::IoFunctions::ftellPtr, [](FILE *) -> long int {
        return 0;
    });
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> freadBackup(&NEO::IoFunctions::freadPtr, [](void *, size_t, size_t, FILE *) -> size_t {
        return 0;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> fcloseBackup(&NEO::IoFunctions::fclosePtr, [](FILE *) -> int {
        return 0;
    });

    auto fileReader = std::unique_ptr<SettingsReader>(SettingsReader::createFileReader());
    EXPECT_NE(nullptr, fileReader.get());

    std::string defaultValue("unk");
    EXPECT_STREQ("unk", fileReader->getSetting("ProductFamilyOverride", defaultValue).c_str());
}

TEST(SettingsReader, GivenSettingsFileExistsWhenCreatingSettingsReaderThenReaderIsCreated) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));

    constexpr std::string_view data = "ProductFamilyOverride = test";
    NEO::writeDataToFile(SettingsReader::settingsFileName, data, false);

    auto reader = std::unique_ptr<SettingsReader>(MockSettingsReader::create(ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
    std::string defaultValue("unk");
    EXPECT_STREQ("test", reader->getSetting("ProductFamilyOverride", defaultValue).c_str());

    removeVirtualFile(SettingsReader::settingsFileName);
}

TEST(SettingsReader, WhenCreatingFileReaderThenReaderIsCreated) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));
    char data = 0;
    std::string_view emptyView(&data, 0);
    NEO::writeDataToFile(SettingsReader::settingsFileName, emptyView, false);

    auto reader = std::unique_ptr<SettingsReader>(MockSettingsReader::createFileReader());
    EXPECT_NE(nullptr, reader.get());

    removeVirtualFile(SettingsReader::settingsFileName);
}

TEST(SettingsReader, WhenCreatingFileReaderUseNeoFileIfNoDefault) {
    ASSERT_FALSE(virtualFileExists(SettingsReader::settingsFileName));
    ASSERT_FALSE(virtualFileExists(SettingsReader::neoSettingsFileName));
    char data = 0;
    std::string_view emptyView(&data, 0);
    NEO::writeDataToFile(SettingsReader::neoSettingsFileName, emptyView, false);

    auto reader = std::unique_ptr<SettingsReader>(MockSettingsReader::createFileReader());
    EXPECT_NE(nullptr, reader.get());

    removeVirtualFile(SettingsReader::neoSettingsFileName);
}

TEST(SettingsReader, WhenCreatingOsReaderThenReaderIsCreated) {
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::createOsReader(false, ApiSpecificConfig::getRegistryPath()));
    EXPECT_NE(nullptr, reader.get());
}

TEST(SettingsReader, GivenRegKeyWhenCreatingOsReaderThenReaderIsCreated) {
    std::string regKey = ApiSpecificConfig::getRegistryPath();
    auto reader = std::unique_ptr<SettingsReader>(SettingsReader::createOsReader(false, regKey));
    EXPECT_NE(nullptr, reader.get());
}

TEST(SettingsReader, GivenTrueWhenPrintingDebugStringThenPrintsToOutput) {
    int i = 4;
    StreamCapture capture;
    capture.captureStdout();
    PRINT_STRING(true, stdout, "testing error %d", i);
    std::string output = capture.getCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
}

TEST(SettingsReader, GivenFalseWhenPrintingDebugStringThenNoOutput) {
    int i = 4;
    StreamCapture capture;
    capture.captureStdout();
    PRINT_STRING(false, stderr, "Error String %d", i);
    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ(output.c_str(), "");
}

TEST(SettingsReader, GivenDebugMessagesBitmaskWithPidWhenPrintingDebugStringThenPrintsPidToOutput) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebugMessagesBitmask.set(DebugMessagesBitmask::withPid);

    int i = 4;
    StreamCapture capture;
    capture.captureStdout();
    PRINT_STRING(true, stdout, "debug string %d", i);
    std::string output = capture.getCapturedStdout();
    EXPECT_TRUE(hasSubstr(output, "[PID: "));
}

TEST(SettingsReader, GivenDebugMessagesBitmaskWithTimestampWhenPrintingDebugStringThenPrintsTimestampToOutput) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.DebugMessagesBitmask.set(DebugMessagesBitmask::withTimestamp);

    int i = 4;
    StreamCapture capture;
    capture.captureStdout();
    PRINT_STRING(true, stdout, "debug string %d", i);
    std::string output = capture.getCapturedStdout();
    std::string dateRegex = R"(\[20\d{2}-\d{2}-\d{2})";
    EXPECT_TRUE(containsRegex(output, dateRegex));
}
} // namespace SettingsReaderTests
