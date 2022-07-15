/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler_tests.h"

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "compiler_options.h"
#include "environment.h"
#include "gtest/gtest.h"
#include "mock/mock_argument_helper.h"
#include "mock/mock_multi_command.h"
#include "mock/mock_offline_compiler.h"
#include "platforms.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <string>

extern Environment *gEnvironment;

namespace NEO {

void MultiCommandTests::createFileWithArgs(const std::vector<std::string> &singleArgs, int numOfBuild) {
    std::ofstream myfile(nameOfFileWithArgs);
    if (myfile.is_open()) {
        for (int i = 0; i < numOfBuild; i++) {
            for (auto singleArg : singleArgs)
                myfile << singleArg + " ";
            myfile << std::endl;
        }
        myfile.close();
    } else
        printf("Unable to open file\n");
}

void MultiCommandTests::deleteFileWithArgs() {
    if (remove(nameOfFileWithArgs.c_str()) != 0)
        perror("Error deleting file");
}

void MultiCommandTests::deleteOutFileList() {
    if (remove(outFileList.c_str()) != 0)
        perror("Error deleting file");
}

std::string getCompilerOutputFileName(const std::string &fileName, const std::string &type) {
    std::string fName(fileName);
    fName.append("_");
    fName.append(gEnvironment->familyNameWithType);
    fName.append(".");
    fName.append(type);
    return fName;
}

bool compilerOutputExists(const std::string &fileName, const std::string &type) {
    return fileExists(getCompilerOutputFileName(fileName, type));
}

void compilerOutputRemove(const std::string &fileName, const std::string &type) {
    std::remove(getCompilerOutputFileName(fileName, type).c_str());
}

template <typename SectionHeaders>
bool isAnyIrSectionDefined(const SectionHeaders &sectionHeaders) {
    const auto isIrSection = [](const auto &section) {
        return section.header && (section.header->type == Elf::SHT_OPENCL_SPIRV || section.header->type == Elf::SHT_OPENCL_LLVM_BINARY);
    };

    return std::any_of(sectionHeaders.begin(), sectionHeaders.end(), isIrSection);
}

TEST_F(MultiCommandTests, WhenBuildingMultiCommandThenSuccessIsReturned) {
    nameOfFileWithArgs = "ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get()));

    EXPECT_NE(nullptr, pMultiCommand);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteFileWithArgs();
}

TEST_F(MultiCommandTests, GivenOutputFileWhenBuildingMultiCommandThenSuccessIsReturned) {
    nameOfFileWithArgs = "ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get()));

    EXPECT_NE(nullptr, pMultiCommand);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (int i = 0; i < numOfBuild; i++) {
        std::string outFileName = pMultiCommand->outDirForBuilds + "/build_no_" + std::to_string(i + 1);
        EXPECT_TRUE(compilerOutputExists(outFileName, "bc") || compilerOutputExists(outFileName, "spv"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "gen"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "bin"));
    }

    deleteFileWithArgs();
}

TEST_F(MultiCommandTests, GivenSpecifiedOutputDirWhenBuildingMultiCommandThenSuccessIsReturned) {
    nameOfFileWithArgs = "ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-out_dir",
        "offline_compiler_test"};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    pMultiCommand = MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pMultiCommand);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (int i = 0; i < numOfBuild; i++) {
        std::string outFileName = "offline_compiler_test/build_no_" + std::to_string(i + 1);
        EXPECT_TRUE(compilerOutputExists(outFileName, "bc") || compilerOutputExists(outFileName, "spv"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "gen"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "bin"));
    }

    deleteFileWithArgs();
    delete pMultiCommand;
}

TEST_F(MultiCommandTests, GivenSpecifiedOutputDirWithProductConfigValueWhenBuildingMultiCommandThenSuccessIsReturned) {
    auto allEnabledDeviceConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    std::string configStr;
    for (auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            configStr = ProductConfigHelper::parseMajorMinorRevisionValue(deviceMapConfig.aotConfig);
            break;
        }
    }

    nameOfFileWithArgs = "ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        configStr,
        "-out_dir",
        "offline_compiler_test"};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    pMultiCommand = MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pMultiCommand);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (int i = 0; i < numOfBuild; i++) {
        std::string outFileName = "offline_compiler_test/build_no_" + std::to_string(i + 1);
        EXPECT_TRUE(compilerOutputExists(outFileName, "bc") || compilerOutputExists(outFileName, "spv"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "gen"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "bin"));
    }

    deleteFileWithArgs();
    delete pMultiCommand;
}

TEST_F(MultiCommandTests, GivenMissingTextFileWithArgsWhenBuildingMultiCommandThenInvalidFileErrorIsReturned) {
    nameOfFileWithArgs = "ImANotExistedComandFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        "ImANaughtyFile.txt",
        "-q",
    };

    testing::internal::CaptureStdout();
    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get()));
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pMultiCommand);
    EXPECT_EQ(OclocErrorCode::INVALID_FILE, retVal);
    DebugManager.flags.PrintDebugMessages.set(false);
}
TEST_F(MultiCommandTests, GivenLackOfClFileWhenBuildingMultiCommandThenInvalidFileErrorIsReturned) {
    nameOfFileWithArgs = "ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clFiles + "ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);
    testing::internal::CaptureStdout();
    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get()));
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(nullptr, pMultiCommand);
    EXPECT_EQ(OclocErrorCode::INVALID_FILE, retVal);
    DebugManager.flags.PrintDebugMessages.set(false);

    deleteFileWithArgs();
}
TEST_F(MultiCommandTests, GivenOutputFileListFlagWhenBuildingMultiCommandThenSuccessIsReturned) {
    nameOfFileWithArgs = "ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
        "-output_file_list",
        "outFileList.txt",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    pMultiCommand = MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pMultiCommand);
    EXPECT_EQ(CL_SUCCESS, retVal);
    outFileList = pMultiCommand->outputFileList;
    EXPECT_TRUE(fileExists(outFileList));

    for (int i = 0; i < numOfBuild; i++) {
        std::string outFileName = pMultiCommand->outDirForBuilds + "/build_no_" + std::to_string(i + 1);
        EXPECT_TRUE(compilerOutputExists(outFileName, "bc") || compilerOutputExists(outFileName, "spv"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "gen"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "bin"));
    }

    deleteFileWithArgs();
    deleteOutFileList();
    delete pMultiCommand;
}

TEST(MultiCommandWhiteboxTest, GivenVerboseModeWhenShowingResultsThenLogsArePrintedForEachBuild) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.retValues = {OclocErrorCode::SUCCESS, OclocErrorCode::INVALID_FILE};
    mockMultiCommand.quiet = false;

    ::testing::internal::CaptureStdout();
    const auto result = mockMultiCommand.showResults();
    const auto output = testing::internal::GetCapturedStdout();

    const auto maskedResult = result | OclocErrorCode::INVALID_FILE;
    EXPECT_NE(OclocErrorCode::SUCCESS, result);
    EXPECT_EQ(OclocErrorCode::INVALID_FILE, maskedResult);

    const auto expectedOutput{"Build command 0: successful\n"
                              "Build command 1: failed. Error code: -5151\n"};
    EXPECT_EQ(expectedOutput, output);
}

TEST(MultiCommandWhiteboxTest, GivenVerboseModeAndDefinedOutputFilenameAndDirectoryWhenAddingAdditionalOptionsToSingleCommandLineThenNothingIsDone) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;

    std::vector<std::string> singleArgs = {
        "-file",
        clFiles + "copybuffer.cl",
        "-output",
        "SpecialOutputFilename",
        "-out_dir",
        "SomeOutputDirectory",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    const auto singleArgsCopy{singleArgs};
    const size_t buildId{0};

    ::testing::internal::CaptureStdout();
    mockMultiCommand.addAdditionalOptionsToSingleCommandLine(singleArgs, buildId);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(singleArgsCopy, singleArgs);
}

TEST(MultiCommandWhiteboxTest, GivenHelpArgumentsWhenInitializingThenHelpIsPrinted) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;

    std::vector<std::string> singleArgs = {
        "--help"};

    const auto args{singleArgs};

    ::testing::internal::CaptureStdout();
    const auto result = mockMultiCommand.initialize(args);
    const auto output = testing::internal::GetCapturedStdout();

    const auto expectedOutput = R"===(Compiles multiple files using a config file.

Usage: ocloc multi <file_name>
  <file_name>   Input file containing a list of arguments for subsequent
                ocloc invocations.
                Expected format of each line inside such file is:
                '-file <filename> -device <device_type> [compile_options]'.
                See 'ocloc compile --help' for available compile_options.
                Results of subsequent compilations will be dumped into 
                a directory with name indentical file_name's base name.

  -output_file_list             Name of optional file containing 
                                paths to outputs .bin files

)===";

    EXPECT_EQ(expectedOutput, output);
    EXPECT_EQ(-1, result);
}

TEST(MultiCommandWhiteboxTest, GivenCommandLineWithApostrophesWhenSplittingLineInSeparateArgsThenTextBetweenApostrophesIsReadAsSingleArg) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;

    const std::string commandLine{" -out_dir \"Some Directory\" -output \'Some Filename\'"};
    std::vector<std::string> outputArgs{};
    const std::size_t numberOfBuild{0};

    ::testing::internal::CaptureStdout();
    const auto result = mockMultiCommand.splitLineInSeparateArgs(outputArgs, commandLine, numberOfBuild);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(OclocErrorCode::SUCCESS, result);
    EXPECT_TRUE(output.empty()) << output;

    ASSERT_EQ(4u, outputArgs.size());

    EXPECT_EQ("-out_dir", outputArgs[0]);
    EXPECT_EQ("Some Directory", outputArgs[1]);

    EXPECT_EQ("-output", outputArgs[2]);
    EXPECT_EQ("Some Filename", outputArgs[3]);
}

TEST(MultiCommandWhiteboxTest, GivenCommandLineWithMissingApostropheWhenSplittingLineInSeparateArgsThenErrorIsReturned) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;

    const std::string commandLine{"-out_dir \"Some Directory"};
    std::vector<std::string> outputArgs{};
    const std::size_t numberOfBuild{0};

    ::testing::internal::CaptureStdout();
    const auto result = mockMultiCommand.splitLineInSeparateArgs(outputArgs, commandLine, numberOfBuild);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(OclocErrorCode::INVALID_FILE, result);

    const auto expectedOutput = "One of the quotes is open in build number 1\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(MultiCommandWhiteboxTest, GivenCommandLineWithMissingApostropheWhenRunningBuildsThenErrorIsReturnedAndBuildIsNotStarted) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = true;
    mockMultiCommand.callBaseSingleBuild = false;
    mockMultiCommand.lines.push_back("-out_dir \"Some Directory");

    mockMultiCommand.runBuilds("ocloc");
    EXPECT_EQ(0, mockMultiCommand.singleBuildCalledCount);

    ASSERT_EQ(1u, mockMultiCommand.retValues.size());
    EXPECT_EQ(OclocErrorCode::INVALID_FILE, mockMultiCommand.retValues[0]);
}

TEST(MultiCommandWhiteboxTest, GivenTwoValidCommandLinesAndVerboseModeWhenRunningBuildsThenBuildsAreStartedReturnValuesAreStoredAndLogsArePrinted) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;
    mockMultiCommand.callBaseSingleBuild = false;

    const std::string validLine{"-file test_files/copybuffer.cl -output SpecialOutputFilename -out_dir SomeOutputDirectory -device " + gEnvironment->devicePrefix};
    mockMultiCommand.lines.push_back(validLine);
    mockMultiCommand.lines.push_back(validLine);

    ::testing::internal::CaptureStdout();
    mockMultiCommand.runBuilds("ocloc");
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(2, mockMultiCommand.singleBuildCalledCount);

    ASSERT_EQ(2u, mockMultiCommand.retValues.size());
    EXPECT_EQ(OclocErrorCode::SUCCESS, mockMultiCommand.retValues[0]);
    EXPECT_EQ(OclocErrorCode::SUCCESS, mockMultiCommand.retValues[1]);

    const auto expectedOutput{"Command number 1: \n"
                              "Command number 2: \n"};
    EXPECT_EQ(expectedOutput, output);
}

TEST(MultiCommandWhiteboxTest, GivenArgsWithQuietModeAndEmptyMulticommandFileWhenInitializingThenQuietFlagIsSetAndErrorIsReturned) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;

    mockMultiCommand.uniqueHelper->callBaseFileExists = false;
    mockMultiCommand.uniqueHelper->callBaseReadFileToVectorOfStrings = false;
    mockMultiCommand.uniqueHelper->shouldReturnEmptyVectorOfStrings = true;
    mockMultiCommand.filesMap["commands.txt"] = "";

    const std::vector<std::string> args = {
        "ocloc",
        "multi",
        "commands.txt",
        "-q"};

    ::testing::internal::CaptureStdout();
    const auto result = mockMultiCommand.initialize(args);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(OclocErrorCode::INVALID_FILE, result);

    const auto expectedOutput = "Command file was empty.\n";
    EXPECT_EQ(expectedOutput, output);

    EXPECT_TRUE(mockMultiCommand.quiet);
}

TEST(MultiCommandWhiteboxTest, GivenInvalidArgsWhenInitializingThenErrorIsReturned) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;

    const std::vector<std::string> args = {
        "ocloc",
        "multi",
        "commands.txt",
        "-invalid_option"};

    ::testing::internal::CaptureStdout();
    const auto result = mockMultiCommand.initialize(args);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, result);

    const auto expectedError = "Invalid option (arg 3): -invalid_option\n";
    const auto errorPosition = output.find(expectedError);
    EXPECT_NE(std::string::npos, errorPosition);
}

using MockOfflineCompilerTests = ::testing::Test;
TEST_F(MockOfflineCompilerTests, givenProductConfigValueAndRevisionIdWhenInitHwInfoThenTheseValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    auto config = AOT::UNKNOWN_ISA;
    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            config = static_cast<AOT::PRODUCT_CONFIG>(deviceMapConfig.aotConfig.ProductConfig);
            break;
        }
    }

    mockOfflineCompiler.revisionId = 0x3;
    mockOfflineCompiler.deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(config);
    EXPECT_FALSE(mockOfflineCompiler.deviceName.empty());

    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, mockOfflineCompiler.revisionId);
    EXPECT_EQ(mockOfflineCompiler.deviceConfig, config);
}

TEST_F(MockOfflineCompilerTests, givenProductConfigValueWhenInitHwInfoThenBaseHardwareInfoValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            mockOfflineCompiler.deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(deviceMapConfig.aotConfig);
            break;
        }
    }

    EXPECT_FALSE(mockOfflineCompiler.deviceName.empty());
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, productFamily);
    EXPECT_NE(mockOfflineCompiler.hwInfo.gtSystemInfo.MaxEuPerSubSlice, 0u);
    EXPECT_NE(mockOfflineCompiler.hwInfo.gtSystemInfo.MaxSlicesSupported, 0u);
    EXPECT_NE(mockOfflineCompiler.hwInfo.gtSystemInfo.MaxSubSlicesSupported, 0u);
}

TEST_F(MockOfflineCompilerTests, givenDeprecatedAcronymsWithRevisionWhenInitHwInfoThenValuesAreSetAndSuccessIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    auto deprecatedAcronyms = mockOfflineCompiler.argHelper->productConfigHelper->getDeprecatedAcronyms();
    if (deprecatedAcronyms.empty()) {
        GTEST_SKIP();
    }

    for (const auto &acronym : deprecatedAcronyms) {
        mockOfflineCompiler.deviceName = acronym.str();
        mockOfflineCompiler.revisionId = 0x3;
        EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OclocErrorCode::SUCCESS);
        EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, mockOfflineCompiler.revisionId);
    }
}

TEST_F(MockOfflineCompilerTests, givenHwInfoConfigWhenSetHwInfoForProductConfigThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto enabledProductAcronyms = mockOfflineCompiler.argHelper->productConfigHelper->getRepresentativeProductAcronyms();
    if (enabledProductAcronyms.empty()) {
        GTEST_SKIP();
    }

    for (const auto &acronym : enabledProductAcronyms) {
        mockOfflineCompiler.deviceName = acronym.str();
        mockOfflineCompiler.hwInfoConfig = 0x100020003;
        EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OclocErrorCode::SUCCESS);

        uint32_t sliceCount = static_cast<uint16_t>(mockOfflineCompiler.hwInfoConfig >> 32);
        uint32_t subSlicePerSliceCount = static_cast<uint16_t>(mockOfflineCompiler.hwInfoConfig >> 16);
        uint32_t euPerSubSliceCount = static_cast<uint16_t>(mockOfflineCompiler.hwInfoConfig);
        uint32_t subSliceCount = subSlicePerSliceCount * sliceCount;
        auto &gtSysInfo = mockOfflineCompiler.hwInfo.gtSystemInfo;

        EXPECT_EQ(gtSysInfo.SliceCount, sliceCount);
        EXPECT_EQ(gtSysInfo.SubSliceCount, subSliceCount);
        EXPECT_EQ(gtSysInfo.EUCount, euPerSubSliceCount * subSliceCount);
        EXPECT_EQ(gtSysInfo.DualSubSliceCount, subSlicePerSliceCount * sliceCount);
        EXPECT_NE(gtSysInfo.ThreadCount, 0u);
    }
}

TEST_F(MockOfflineCompilerTests, givenHwInfoConfigWhenSetHwInfoForDeprecatedAcronymsThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto deprecatedAcronyms = mockOfflineCompiler.argHelper->productConfigHelper->getDeprecatedAcronyms();
    if (deprecatedAcronyms.empty()) {
        GTEST_SKIP();
    }
    for (const auto &acronym : deprecatedAcronyms) {
        mockOfflineCompiler.deviceName = acronym.str();
        mockOfflineCompiler.hwInfoConfig = 0x100020003;

        EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OclocErrorCode::SUCCESS);

        uint32_t sliceCount = static_cast<uint16_t>(mockOfflineCompiler.hwInfoConfig >> 32);
        uint32_t subSlicePerSliceCount = static_cast<uint16_t>(mockOfflineCompiler.hwInfoConfig >> 16);
        uint32_t euPerSubSliceCount = static_cast<uint16_t>(mockOfflineCompiler.hwInfoConfig);
        uint32_t subSliceCount = subSlicePerSliceCount * sliceCount;

        auto &gtSysInfo = mockOfflineCompiler.hwInfo.gtSystemInfo;
        EXPECT_EQ(gtSysInfo.SliceCount, sliceCount);
        EXPECT_EQ(gtSysInfo.SubSliceCount, subSliceCount);
        EXPECT_EQ(gtSysInfo.EUCount, euPerSubSliceCount * subSliceCount);
        EXPECT_NE(gtSysInfo.ThreadCount, 0u);
    }
}

TEST_F(MockOfflineCompilerTests, givenAcronymWithUppercaseWhenInitHwInfoThenSuccessIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    auto allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            if (!deviceMapConfig.acronyms.empty()) {
                mockOfflineCompiler.deviceName = deviceMapConfig.acronyms.front().str();
                break;
            }
        }
    }
    if (mockOfflineCompiler.deviceName.empty()) {
        GTEST_SKIP();
    }

    std::transform(mockOfflineCompiler.deviceName.begin(), mockOfflineCompiler.deviceName.end(), mockOfflineCompiler.deviceName.begin(), ::toupper);
    EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OclocErrorCode::SUCCESS);
}

TEST_F(MockOfflineCompilerTests, givenDeprecatedAcronymsWithUppercaseWhenInitHwInfoThenSuccessIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    auto deprecatedAcronyms = mockOfflineCompiler.argHelper->productConfigHelper->getDeprecatedAcronyms();
    if (deprecatedAcronyms.empty()) {
        GTEST_SKIP();
    }

    for (const auto &acronym : deprecatedAcronyms) {
        mockOfflineCompiler.deviceName = acronym.str();
        std::transform(mockOfflineCompiler.deviceName.begin(), mockOfflineCompiler.deviceName.end(), mockOfflineCompiler.deviceName.begin(), ::toupper);
        EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OclocErrorCode::SUCCESS);
    }
}

HWTEST2_F(MockOfflineCompilerTests, givenProductConfigValueWhenInitHwInfoThenMaxDualSubSlicesSupportedIsSet, IsAtLeastGen12lp) {
    MockOfflineCompiler mockOfflineCompiler;
    auto allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            mockOfflineCompiler.deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(deviceMapConfig.aotConfig);
            break;
        }
    }

    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    EXPECT_NE(mockOfflineCompiler.hwInfo.gtSystemInfo.MaxDualSubSlicesSupported, 0u);
}

TEST_F(OfflineCompilerTests, GivenHelpOptionOnQueryThenSuccessIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "query",
        "--help"};

    testing::internal::CaptureStdout();
    int retVal = OfflineCompiler::query(argv.size(), argv, oclocArgHelperWithoutInput.get());
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_STREQ(OfflineCompiler::queryHelp.data(), output.c_str());
    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
}

TEST_F(OfflineCompilerTests, GivenHelpOptionOnIdsThenSuccessIsReturned) {
    std::vector<ConstStringRef> helpFlags = {"-h", "--help"};
    for (const auto &helpFlag : helpFlags) {
        std::vector<std::string> argv = {
            "ocloc",
            "ids",
            helpFlag.str()};

        testing::internal::CaptureStdout();
        int retVal = OfflineCompiler::queryAcronymIds(argv.size(), argv, oclocArgHelperWithoutInput.get());
        std::string output = testing::internal::GetCapturedStdout();

        std::stringstream expectedOutput;
        expectedOutput << R"===(
Depending on <acronym> will return all
matched versions (<major>.<minor>.<revision>)
that correspond to the given name.
All supported acronyms: )===";
        expectedOutput << getSupportedDevices(oclocArgHelperWithoutInput.get()) << ".\n";

        EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
        EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    }
}

TEST_F(OfflineCompilerTests, givenFamilyAcronymWhenIdsCommandIsInvokeThenSuccessAndCorrectIdsAreReturned) {
    auto enabledFamilies = oclocArgHelperWithoutInput->productConfigHelper->getFamiliesAcronyms();
    if (enabledFamilies.empty()) {
        GTEST_SKIP();
    }
    auto supportedDevicesConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    for (const auto &familyAcronym : enabledFamilies) {
        std::vector<std::string> expected{};
        auto family = ProductConfigHelper::getFamilyForAcronym(familyAcronym.str());
        for (const auto &device : supportedDevicesConfigs) {
            if (device.family == family) {
                auto config = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
                expected.push_back(config);
            }
        }
        std::vector<std::string> argv = {
            "ocloc",
            "ids",
            familyAcronym.str()};

        std::stringstream expectedOutput;
        testing::internal::CaptureStdout();
        int retVal = OfflineCompiler::queryAcronymIds(argv.size(), argv, oclocArgHelperWithoutInput.get());
        std::string output = testing::internal::GetCapturedStdout();
        expectedOutput << "Matched ids:\n";

        for (const auto &prefix : expected) {
            expectedOutput << prefix << "\n";
        }
        EXPECT_STREQ(expectedOutput.str().c_str(), output.c_str());
        EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    }
}

TEST_F(OfflineCompilerTests, givenReleaseAcronymWhenIdsCommandIsInvokeThenSuccessAndCorrectIdsAreReturned) {
    auto enabledReleases = oclocArgHelperWithoutInput->productConfigHelper->getReleasesAcronyms();
    if (enabledReleases.empty()) {
        GTEST_SKIP();
    }
    auto supportedDevicesConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    for (const auto &releaseAcronym : enabledReleases) {
        std::vector<std::string> expected{};
        auto release = ProductConfigHelper::getReleaseForAcronym(releaseAcronym.str());
        for (const auto &device : supportedDevicesConfigs) {
            if (device.release == release) {
                auto config = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
                expected.push_back(config);
            }
        }
        std::vector<std::string> argv = {
            "ocloc",
            "ids",
            releaseAcronym.str()};

        std::stringstream expectedOutput;
        testing::internal::CaptureStdout();
        int retVal = OfflineCompiler::queryAcronymIds(argv.size(), argv, oclocArgHelperWithoutInput.get());
        std::string output = testing::internal::GetCapturedStdout();
        expectedOutput << "Matched ids:\n";

        for (const auto &prefix : expected) {
            expectedOutput << prefix << "\n";
        }
        EXPECT_STREQ(expectedOutput.str().c_str(), output.c_str());
        EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    }
}

TEST_F(OfflineCompilerTests, givenProductAcronymWhenIdsCommandIsInvokeThenSuccessAndCorrectIdsAreReturned) {
    auto enabledProducts = oclocArgHelperWithoutInput->productConfigHelper->getAllProductAcronyms();
    if (enabledProducts.empty()) {
        GTEST_SKIP();
    }
    auto supportedDevicesConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    for (const auto &productAcronym : enabledProducts) {
        std::vector<std::string> expected{};
        auto product = ProductConfigHelper::getProductConfigForAcronym(productAcronym.str());
        for (const auto &device : supportedDevicesConfigs) {
            if (device.aotConfig.ProductConfig == product) {
                auto config = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
                expected.push_back(config);
            }
        }
        std::vector<std::string> argv = {
            "ocloc",
            "ids",
            productAcronym.str()};

        std::stringstream expectedOutput;
        testing::internal::CaptureStdout();
        int retVal = OfflineCompiler::queryAcronymIds(argv.size(), argv, oclocArgHelperWithoutInput.get());
        std::string output = testing::internal::GetCapturedStdout();
        expectedOutput << "Matched ids:\n";

        for (const auto &prefix : expected) {
            expectedOutput << prefix << "\n";
        }
        EXPECT_STREQ(expectedOutput.str().c_str(), output.c_str());
        EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    }
}

TEST_F(OfflineCompilerTests, GivenFlagsWhichRequireMoreArgsWithoutThemWhenParsingThenErrorIsReported) {
    const std::array<std::string, 8> flagsToTest = {
        "-file", "-output", "-device", "-options", "-internal_options", "-out_dir", "-revision_id", "-config"};

    for (const auto &flag : flagsToTest) {
        const std::vector<std::string> argv = {
            "ocloc",
            "compile",
            flag};

        MockOfflineCompiler mockOfflineCompiler{};

        ::testing::internal::CaptureStdout();
        const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
        const auto output{::testing::internal::GetCapturedStdout()};

        EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, result);

        const std::string expectedErrorMessage{"Invalid option (arg 2): " + flag + "\n"};
        EXPECT_EQ(expectedErrorMessage, output);
    }
}

TEST_F(OfflineCompilerTests, Given32BitModeFlagWhenParsingThenInternalOptionsContain32BitModeFlag) {
    const std::array<std::string, 2> flagsToTest = {
        "-32", CompilerOptions::arch32bit.str()};

    for (const auto &flag : flagsToTest) {
        const std::vector<std::string> argv = {
            "ocloc",
            "compile",
            "-file",
            clFiles + "copybuffer.cl",
            flag,
            "-device",
            gEnvironment->devicePrefix.c_str()};

        MockOfflineCompiler mockOfflineCompiler{};

        const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
        EXPECT_EQ(OclocErrorCode::SUCCESS, result);

        const auto is32BitModeSet{mockOfflineCompiler.internalOptions.find(CompilerOptions::arch32bit.data()) != std::string::npos};
        EXPECT_TRUE(is32BitModeSet);
    }
}

TEST_F(OfflineCompilerTests, givenConfigFlagWhenParsingCommandLineThenCorrectValueIsSet) {
    std::string configStr = "1x2x3";
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-config",
        configStr};

    MockOfflineCompiler mockOfflineCompiler{};
    uint64_t config = 0u;
    parseHwInfoConfigString(configStr, config);

    const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, result);
    EXPECT_EQ(mockOfflineCompiler.hwInfoConfig, config);
}

TEST_F(OfflineCompilerTests, givenIncorrectConfigFlagWhenParsingCommandLineThenErrorLogIsPrintedAndFailureIsReturned) {
    std::string configStr = "1xabcf";
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-config",
        configStr};

    MockOfflineCompiler mockOfflineCompiler{};

    ::testing::internal::CaptureStdout();
    const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, result);
    EXPECT_EQ(mockOfflineCompiler.hwInfoConfig, 0u);

    const std::string expectedErrorMessage{"Error: Invalid config.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineCompilerTests, Given64BitModeFlagWhenParsingThenInternalOptionsContain64BitModeFlag) {
    const std::array<std::string, 2> flagsToTest = {
        "-64", CompilerOptions::arch64bit.str()};

    for (const auto &flag : flagsToTest) {
        const std::vector<std::string> argv = {
            "ocloc",
            "compile",
            "-file",
            clFiles + "copybuffer.cl",
            flag,
            "-device",
            gEnvironment->devicePrefix.c_str()};

        MockOfflineCompiler mockOfflineCompiler{};

        const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
        EXPECT_EQ(OclocErrorCode::SUCCESS, result);

        const auto is64BitModeSet{mockOfflineCompiler.internalOptions.find(CompilerOptions::arch64bit.data()) != std::string::npos};
        EXPECT_TRUE(is64BitModeSet);
    }
}

TEST_F(OfflineCompilerTests, Given32BitModeFlagAnd64BitModeFlagWhenParsingThenErrorLogIsPrintedAndFailureIsReturned) {
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        clFiles + "copybuffer.cl",
        "-32",
        "-64",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    ::testing::internal::CaptureStdout();
    const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_NE(OclocErrorCode::SUCCESS, result);

    const auto maskedResult = result | OclocErrorCode::INVALID_COMMAND_LINE;
    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, maskedResult);

    const std::string expectedErrorMessage{"Error: Cannot compile for 32-bit and 64-bit, please choose one.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineCompilerTests, GivenFlagStringWhenParsingThenInternalBooleanIsSetAndSuccessIsReturned) {
    using namespace std::string_literals;

    const std::array flagsToTest = {
        std::pair{"-options_name"s, &MockOfflineCompiler::useOptionsSuffix},
        std::pair{"-gen_file"s, &MockOfflineCompiler::useGenFile},
        std::pair{"-llvm_bc"s, &MockOfflineCompiler::useLlvmBc}};

    for (const auto &[flagString, memberBoolean] : flagsToTest) {
        const std::vector<std::string> argv = {
            "ocloc",
            "compile",
            "-file",
            clFiles + "copybuffer.cl",
            flagString,
            "-device",
            gEnvironment->devicePrefix.c_str()};

        MockOfflineCompiler mockOfflineCompiler{};

        const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
        EXPECT_EQ(OclocErrorCode::SUCCESS, result);

        EXPECT_TRUE(mockOfflineCompiler.*memberBoolean);
    }
}

TEST_F(OfflineCompilerTests, GivenArgsWhenQueryIsCalledThenSuccessIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "query",
        "NEO_REVISION"};

    int retVal = OfflineCompiler::query(argv.size(), argv, oclocArgHelperWithoutInput.get());

    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
}

TEST_F(OfflineCompilerTests, GivenArgsWhenOfflineCompilerIsCreatedThenSuccessIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, givenDeviceIdHexValueWhenInitHwInfoThenItHasCorrectlySetValues) {
    auto deviceId = oclocArgHelperWithoutInput->deviceProductTable[0].deviceId;
    if (oclocArgHelperWithoutInput->deviceProductTable.size() == 1 && deviceId == 0) {
        GTEST_SKIP();
    }

    MockOfflineCompiler mockOfflineCompiler;
    std::stringstream deviceString, productString;
    deviceString << "0x" << std::hex << deviceId;

    mockOfflineCompiler.argHelper->getPrinterRef() = MessagePrinter{true};
    mockOfflineCompiler.initHardwareInfo(deviceString.str());
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceId);
}

TEST_F(OfflineCompilerTests, givenProperDeviceIdHexAsDeviceArgumentThenSuccessIsReturned) {
    auto deviceId = oclocArgHelperWithoutInput->deviceProductTable[0].deviceId;
    if (oclocArgHelperWithoutInput->deviceProductTable.size() == 1 && deviceId == 0) {
        GTEST_SKIP();
    }

    std::stringstream deviceString, productString;

    deviceString << "0x" << std::hex << deviceId;
    productString << oclocArgHelperWithoutInput->deviceProductTable[0].product;

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        deviceString.str()};

    oclocArgHelperWithoutInput->getPrinterRef() = MessagePrinter{false};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(pOfflineCompiler->getHardwareInfo().platform.usDeviceID, deviceId);

    auto output = testing::internal::GetCapturedStdout();
    std::stringstream resString;
    resString << "Auto-detected target based on " << deviceString.str() << " device id: " << productString.str() << "\n";

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), resString.str().c_str());
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, givenIncorrectDeviceIdHexThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        "0x0"};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine target based on device id: 0x0\nError: Cannot get HW Info for device 0x0.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWithMissingRevisionValueWhenInvalidPatternIsPassedThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        "9.1."};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 9.1.\nError: Cannot get HW Info for device 9.1..\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWithInvalidPatternThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        "9.1.."};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 9.1..\nError: Cannot get HW Info for device 9.1...\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWithMissingMajorValueWhenInvalidPatternIsPassedThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        ".1.2"};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: .1.2\nError: Cannot get HW Info for device .1.2.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWhenInvalidRevisionValueIsPassedThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        "9.0.a"};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 9.0.a\nError: Cannot get HW Info for device 9.0.a.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWhenInvalidMinorValueIsPassedThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        "9.a"};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 9.a\nError: Cannot get HW Info for device 9.a.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWhenPassedValuesAreOutOfRangeThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        "256.350"};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 256.350\nError: Cannot get HW Info for device 256.350.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenInitHardwareInfowhenDeviceConfigContainsDeviceIdsThenSetFirstDeviceId) {
    MockOfflineCompiler mockOfflineCompiler;
    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }
    std::vector<unsigned short> deviceIdsForTests = {0xfffd, 0xfffe, 0xffff};

    for (auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            mockOfflineCompiler.deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(deviceMapConfig.aotConfig);
            deviceMapConfig.deviceIds = &deviceIdsForTests;
            break;
        }
    }

    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.eProductFamily, productFamily);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceIdsForTests.front());
}

TEST_F(OfflineCompilerTests, givenIncorrectDeviceIdWithIncorrectHexPatternThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        "0xnonexist"};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Error: Cannot get HW Info for device 0xnonexist.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDebugOptionThenInternalOptionShouldContainKernelDebugEnable) {
    if (gEnvironment->devicePrefix == "bdw") {
        GTEST_SKIP();
    }

    std::vector<std::string> argv = {
        "ocloc",
        "-options",
        "-g",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->initialize(argv.size(), argv);

    std::string internalOptions = mockOfflineCompiler->internalOptions;
    EXPECT_TRUE(hasSubstr(internalOptions, "-cl-kernel-debug-enable"));
}

TEST_F(OfflineCompilerTests, givenDashGInBiggerOptionStringWhenInitializingThenInternalOptionsShouldNotContainKernelDebugEnable) {

    std::vector<std::string> argv = {
        "ocloc",
        "-options",
        "-gNotRealDashGOption",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->initialize(argv.size(), argv);

    std::string internalOptions = mockOfflineCompiler->internalOptions;
    EXPECT_FALSE(hasSubstr(internalOptions, "-cl-kernel-debug-enable"));
}

TEST_F(OfflineCompilerTests, givenExcludeIrFromZebinInternalOptionWhenInitIsPerformedThenIrExcludeFlagsShouldBeUnified) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-internal_options",
        "-ze-allow-zebin -ze-exclude-ir-from-zebin",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    EXPECT_TRUE(mockOfflineCompiler.excludeIr);
}

TEST_F(OfflineCompilerTests, givenValidArgumentsAndFclInitFailureWhenInitIsPerformedThenFailureIsPropagatedAndErrorIsPrinted) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.mockFclFacade->shouldFailLoadingOfFclLib = true;

    testing::internal::CaptureStdout();
    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    const auto output = testing::internal::GetCapturedStdout();

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error! Loading of FCL library has failed! Filename: " << Os::frontEndDllName << "\n"
                         << "Error! FCL initialization failure. Error code = -6\n";

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, initResult);
    EXPECT_EQ(expectedErrorMessage.str(), output);
}

TEST_F(OfflineCompilerTests, givenValidArgumentsAndIgcInitFailureWhenInitIsPerformedThenFailureIsPropagatedAndErrorIsPrinted) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.mockIgcFacade->shouldFailLoadingOfIgcLib = true;

    testing::internal::CaptureStdout();
    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    const auto output = testing::internal::GetCapturedStdout();

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error! Loading of IGC library has failed! Filename: " << Os::igcDllName << "\n"
                         << "Error! IGC initialization failure. Error code = -6\n";

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, initResult);
    EXPECT_EQ(expectedErrorMessage.str(), output);
}

TEST_F(OfflineCompilerTests, givenExcludeIrArgumentWhenInitIsPerformedThenIrExcludeFlagsShouldBeUnified) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-exclude_ir",
        "-internal_options",
        "-ze-allow-zebin",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const auto expectedInternalOption{"-ze-exclude-ir-from-zebin"};
    const auto excludeIrFromZebinEnabled{mockOfflineCompiler.internalOptions.find(expectedInternalOption) != std::string::npos};
    EXPECT_TRUE(excludeIrFromZebinEnabled);
}

TEST_F(OfflineCompilerTests, givenExcludeIrArgumentAndExcludeIrFromZebinInternalOptionWhenInitIsPerformedThenExcludeIrFromZebinInternalOptionOccursJustOnce) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-exclude_ir",
        "-internal_options",
        "-ze-allow-zebin -ze-exclude-ir-from-zebin",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const auto expectedInternalOption{"-ze-exclude-ir-from-zebin"};
    const auto firstExcludeIrFromZebin{mockOfflineCompiler.internalOptions.find(expectedInternalOption)};
    ASSERT_NE(std::string::npos, firstExcludeIrFromZebin);

    const auto lastExcludeIrFromZebin{mockOfflineCompiler.internalOptions.rfind(expectedInternalOption)};
    EXPECT_EQ(firstExcludeIrFromZebin, lastExcludeIrFromZebin);
}

TEST_F(OfflineCompilerTests, givenExcludeIrArgumentWhenCompilingKernelThenIrShouldBeExcluded) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-exclude_ir",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const auto buildResult{mockOfflineCompiler.build()};
    ASSERT_EQ(OclocErrorCode::SUCCESS, buildResult);

    std::string errorReason{};
    std::string warning{};

    const auto elf{Elf::decodeElf(mockOfflineCompiler.elfBinary, errorReason, warning)};
    ASSERT_TRUE(errorReason.empty());
    ASSERT_TRUE(warning.empty());

    EXPECT_FALSE(isAnyIrSectionDefined(elf.sectionHeaders));
}

TEST_F(OfflineCompilerTests, givenLackOfExcludeIrArgumentWhenCompilingKernelThenIrShouldBeIncluded) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);

    const auto buildResult{mockOfflineCompiler.build()};
    ASSERT_EQ(OclocErrorCode::SUCCESS, buildResult);

    std::string errorReason{};
    std::string warning{};

    const auto elf{Elf::decodeElf(mockOfflineCompiler.elfBinary, errorReason, warning)};
    ASSERT_TRUE(errorReason.empty());
    ASSERT_TRUE(warning.empty());

    EXPECT_TRUE(isAnyIrSectionDefined(elf.sectionHeaders));
}

TEST_F(OfflineCompilerTests, givenZeroSizeInputFileWhenInitializationIsPerformedThenInvalidFileIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->callBaseLoadDataFromFile = false;
    mockOfflineCompiler.uniqueHelper->shouldLoadDataFromFileReturnZeroSize = true;

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::INVALID_FILE, initResult);
}

TEST_F(OfflineCompilerTests, givenInvalidIgcOutputWhenCompilingKernelThenOutOfHostMemoryIsReturned) {
    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.shouldReturnInvalidTranslationOutput = true;
    NEO::setIgcDebugVars(igcDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, buildResult);

    NEO::setIgcDebugVars(gEnvironment->igcDebugVars);
}

TEST_F(OfflineCompilerTests, givenIgcBuildFailureWhenCompilingKernelThenBuildProgramFailureIsReturned) {
    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.forceBuildFailure = true;
    NEO::setIgcDebugVars(igcDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OclocErrorCode::BUILD_PROGRAM_FAILURE, buildResult);

    NEO::setIgcDebugVars(gEnvironment->igcDebugVars);
}

TEST_F(OfflineCompilerTests, givenInvalidFclOutputWhenCompilingKernelThenOutOfHostMemoryIsReturned) {
    MockCompilerDebugVars fclDebugVars{gEnvironment->fclDebugVars};
    fclDebugVars.shouldReturnInvalidTranslationOutput = true;
    NEO::setFclDebugVars(fclDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, buildResult);

    NEO::setFclDebugVars(gEnvironment->fclDebugVars);
}

TEST_F(OfflineCompilerTests, givenFclTranslationContextCreationFailureWhenCompilingKernelThenOutOfHostMemoryIsReturned) {
    MockCompilerDebugVars fclDebugVars{gEnvironment->fclDebugVars};
    fclDebugVars.shouldFailCreationOfTranslationContext = true;
    NEO::setFclDebugVars(fclDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, buildResult);

    NEO::setFclDebugVars(gEnvironment->fclDebugVars);
}

TEST_F(OfflineCompilerTests, givenFclTranslationContextCreationFailureAndErrorMessageWhenCompilingKernelThenProgramBuildFailureIsReturnedAndBuildLogIsUpdated) {
    MockCompilerDebugVars fclDebugVars{gEnvironment->fclDebugVars};
    fclDebugVars.shouldFailCreationOfTranslationContext = true;
    fclDebugVars.translationContextCreationError = "Error: Could not create context!";
    NEO::setFclDebugVars(fclDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OclocErrorCode::BUILD_PROGRAM_FAILURE, buildResult);

    EXPECT_EQ("Error: Could not create context!", mockOfflineCompiler.getBuildLog());

    NEO::setFclDebugVars(gEnvironment->fclDebugVars);
}

TEST_F(OfflineCompilerTests, givenVariousClStdValuesWhenCompilingSourceThenCorrectExtensionsArePassed) {
    std::string clStdOptionValues[] = {"", "-cl-std=CL1.2", "-cl-std=CL2.0", "-cl-std=CL3.0"};

    for (auto &clStdOptionValue : clStdOptionValues) {
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            gEnvironment->devicePrefix.c_str()};

        if (!clStdOptionValue.empty()) {
            argv.push_back("-options");
            argv.push_back(clStdOptionValue);
        }

        auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
        ASSERT_NE(nullptr, mockOfflineCompiler);
        mockOfflineCompiler->initialize(argv.size(), argv);

        std::string internalOptions = mockOfflineCompiler->internalOptions;
        std::string oclVersionOption = getOclVersionCompilerInternalOption(mockOfflineCompiler->hwInfo.capabilityTable.clVersionSupport);
        EXPECT_TRUE(hasSubstr(internalOptions, oclVersionOption));

        if (clStdOptionValue == "-cl-std=CL2.0") {
            auto expectedRegex = std::string{"cl_khr_3d_image_writes"};
            if (mockOfflineCompiler->hwInfo.capabilityTable.supportsImages) {
                expectedRegex += ".+" + std::string{"cl_khr_3d_image_writes"};
            }
            EXPECT_TRUE(containsRegex(internalOptions, expectedRegex));
        }

        OpenClCFeaturesContainer openclCFeatures;
        getOpenclCFeaturesList(mockOfflineCompiler->hwInfo, openclCFeatures);
        for (auto &feature : openclCFeatures) {
            if (clStdOptionValue == "-cl-std=CL3.0") {
                EXPECT_TRUE(hasSubstr(internalOptions, std::string{feature.name}));
            } else {
                EXPECT_FALSE(hasSubstr(internalOptions, std::string{feature.name}));
            }
        }

        if (mockOfflineCompiler->hwInfo.capabilityTable.supportsImages) {
            EXPECT_TRUE(hasSubstr(internalOptions, CompilerOptions::enableImageSupport.data()));
        } else {
            EXPECT_FALSE(hasSubstr(internalOptions, CompilerOptions::enableImageSupport.data()));
        }
    }
}

TEST_F(OfflineCompilerTests, GivenArgsWhenBuildingThenBuildSucceeds) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    testing::internal::CaptureStdout();
    retVal = pOfflineCompiler->build();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("copybuffer", "bc") || compilerOutputExists("copybuffer", "spv"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "gen"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "bin"));

    std::string buildLog = pOfflineCompiler->getBuildLog();
    EXPECT_STREQ(buildLog.c_str(), "");

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenArgsWhenBuildingWithDeviceConfigValueThenBuildSucceeds) {
    auto allEnabledDeviceConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        return;
    }

    std::string configStr;
    for (auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            configStr = ProductConfigHelper::parseMajorMinorRevisionValue(deviceMapConfig.aotConfig);
            break;
        }
    }

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        configStr};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    testing::internal::CaptureStdout();
    retVal = pOfflineCompiler->build();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("copybuffer", "bc") || compilerOutputExists("copybuffer", "spv"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "gen"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "bin"));

    std::string buildLog = pOfflineCompiler->getBuildLog();
    EXPECT_STREQ(buildLog.c_str(), "");

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenLlvmTextWhenBuildingThenBuildSucceeds) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-llvm_text"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("copybuffer", "ll"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "gen"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "bin"));

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, WhenFclNotNeededThenDontLoadIt) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spirv_input"};

    MockOfflineCompiler offlineCompiler;
    auto ret = offlineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(offlineCompiler.fclFacade->isInitialized());
    EXPECT_TRUE(offlineCompiler.igcFacade->isInitialized());
}

TEST_F(OfflineCompilerTests, WhenParsingBinToCharArrayThenCorrectResult) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    // clang-format off
    uint8_t binary[] = {
        0x02, 0x23, 0x3, 0x40, 0x56, 0x7, 0x80, 0x90, 0x1, 0x03,
        0x34, 0x5, 0x60, 0x78, 0x9, 0x66, 0xff, 0x10, 0x10, 0x10,
        0x02, 0x23, 0x3, 0x40, 0x56, 0x7, 0x80, 0x90, 0x1, 0x03,
        0x34, 0x5, 0x60, 0x78, 0x9, 0x66, 0xff,
    };
    // clang-format on

    std::string familyNameWithType = gEnvironment->familyNameWithType;
    std::string fileName = "scheduler";
    std::string retArray = pOfflineCompiler->parseBinAsCharArray(binary, sizeof(binary), fileName);
    std::string target = "#include <cstddef>\n"
                         "#include <cstdint>\n\n"
                         "size_t SchedulerBinarySize_" +
                         familyNameWithType + " = 37;\n"
                                              "uint32_t SchedulerBinary_" +
                         familyNameWithType + "[10] = {\n"
                                              "    0x40032302, 0x90800756, 0x05340301, 0x66097860, 0x101010ff, 0x40032302, 0x90800756, 0x05340301, \n"
                                              "    0x66097860, 0xff000000};\n\n"
                                              "#include \"shared/source/built_ins/registry/built_ins_registry.h\"\n\n"
                                              "namespace NEO {\n"
                                              "static RegisterEmbeddedResource registerSchedulerBin(\n"
                                              "    \"" +
                         gEnvironment->familyNameWithType + "_0_scheduler.builtin_kernel.bin\",\n"
                                                            "    (const char *)SchedulerBinary_" +
                         familyNameWithType + ",\n"
                                              "    SchedulerBinarySize_" +
                         familyNameWithType + ");\n"
                                              "}\n";
    EXPECT_EQ(retArray, target);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenCppFileWhenBuildingThenBuildSucceeds) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-cpp_file"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("copybuffer", "cpp"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "bc") || compilerOutputExists("copybuffer", "spv"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "gen"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "bin"));

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, GivenOutputDirWhenBuildingThenBuildSucceeds) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-out_dir",
        "offline_compiler_test"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/copybuffer", "bc") || compilerOutputExists("offline_compiler_test/copybuffer", "spv"));
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/copybuffer", "gen"));
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/copybuffer", "bin"));

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, GivenHelpOptionThenBuildDoesNotOccur) {
    std::vector<std::string> argv = {
        "ocloc",
        "--help"};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE("", output.c_str());
    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, GivenInvalidFileWhenBuildingThenInvalidFileErrorIsReturned) {
    DebugManager.flags.PrintDebugMessages.set(true);
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(OclocErrorCode::INVALID_FILE, retVal);
    DebugManager.flags.PrintDebugMessages.set(false);
    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenInvalidFlagWhenBuildingThenInvalidCommandLineErrorIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-n",
        clFiles + "ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenInvalidOptionsWhenBuildingThenInvalidCommandLineErrorIsReturned) {
    std::vector<std::string> argvA = {
        "ocloc",
        "-file",
    };

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argvA.size(), argvA, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");

    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;

    std::vector<std::string> argvB = {
        "ocloc",
        "-file",
        clFiles + "ImANaughtyFile.cl",
        "-device"};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argvB.size(), argvB, true, retVal, oclocArgHelperWithoutInput.get());
    output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenNonexistantDeviceWhenCompilingThenInvalidDeviceErrorAndErrorMessageAreReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        "foobar"};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "Error: Cannot get HW Info for device foobar.\n");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, GivenInvalidKernelWhenBuildingThenBuildProgramFailureErrorIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "shouldfail.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    gEnvironment->SetInputFileName("invalid_file_name");
    testing::internal::CaptureStdout();

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "");

    std::string buildLog = pOfflineCompiler->getBuildLog();
    EXPECT_STRNE(buildLog.c_str(), "");

    gEnvironment->SetInputFileName("copybuffer");

    delete pOfflineCompiler;
}

TEST(OfflineCompilerTest, WhenParsingCmdLineThenOptionsAreReadCorrectly) {
    std::vector<std::string> argv = {
        "ocloc",
        NEO::CompilerOptions::greaterThan4gbBuffersRequired.data()};

    MockOfflineCompiler *mockOfflineCompiler = new MockOfflineCompiler();
    ASSERT_NE(nullptr, mockOfflineCompiler);

    testing::internal::CaptureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = testing::internal::GetCapturedStdout();

    std::string internalOptions = mockOfflineCompiler->internalOptions;
    size_t found = internalOptions.find(argv.begin()[1]);
    EXPECT_NE(std::string::npos, found);

    delete mockOfflineCompiler;
}

TEST(OfflineCompilerTest, GivenUnknownIsaConfigValueWhenInitHardwareInfoThenInvalidDeviceIsReturned) {
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

    auto deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(AOT::UNKNOWN_ISA);
    std::stringstream resString;

    testing::internal::CaptureStdout();
    auto retVal = mockOfflineCompiler->initHardwareInfoForProductConfig(deviceName);
    EXPECT_EQ(retVal, OclocErrorCode::INVALID_DEVICE);

    auto output = testing::internal::GetCapturedStdout();
    resString << "Could not determine device target: " << deviceName << "\n";
    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST(OfflineCompilerTest, GivenUnsupportedDeviceConfigWhenInitHardwareInfoThenInvalidDeviceIsReturned) {
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

    auto deviceName = "00.01.02";
    std::stringstream resString;

    testing::internal::CaptureStdout();
    auto retVal = mockOfflineCompiler->initHardwareInfoForProductConfig(deviceName);
    EXPECT_EQ(retVal, OclocErrorCode::INVALID_DEVICE);

    auto output = testing::internal::GetCapturedStdout();
    resString << "Could not determine target based on product config: " << deviceName << "\n";
    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST(OfflineCompilerTest, givenStatelessToStatefulOptimizationEnabledWhenDebugSettingsAreParsedThenOptimizationStringIsPresent) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.internalOptions;
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, givenStatelessToStatefullOptimizationEnabledWhenDebugSettingsAreParsedThenOptimizationStringIsSetToDefault) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(-1);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.internalOptions;
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, GivenDelimitersWhenGettingStringThenParseIsCorrect) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::string kernelFileName(sharedBuiltinsDir + "/copy_buffer_to_buffer.builtin_kernel");

    size_t srcSize = 0;
    auto ptrSrc = loadDataFromFile(kernelFileName.c_str(), srcSize);

    const std::string src = ptrSrc.get();
    ASSERT_EQ(srcSize, src.size());

    // assert that pattern was found
    ASSERT_NE(std::string::npos, src.find("R\"===("));
    ASSERT_NE(std::string::npos, src.find(")===\""));

    auto dst = mockOfflineCompiler->getStringWithinDelimiters(src);

    size_t size = dst.size();
    char nullChar = '\0';
    EXPECT_EQ(nullChar, dst[size - 1]);

    // expect that pattern was not found
    EXPECT_EQ(std::string::npos, dst.find("R\"===("));
    EXPECT_EQ(std::string::npos, dst.find(")===\""));
}

TEST(OfflineCompilerTest, WhenConvertingToPascalCaseThenResultIsCorrect) {
    EXPECT_EQ(0, strcmp("AuxTranslation", convertToPascalCase("aux_translation").c_str()));
    EXPECT_EQ(0, strcmp("CopyBufferToBuffer", convertToPascalCase("copy_buffer_to_buffer").c_str()));
    EXPECT_EQ(0, strcmp("CopyBufferRect", convertToPascalCase("copy_buffer_rect").c_str()));
    EXPECT_EQ(0, strcmp("FillBuffer", convertToPascalCase("fill_buffer").c_str()));
    EXPECT_EQ(0, strcmp("CopyBufferToImage3d", convertToPascalCase("copy_buffer_to_image3d").c_str()));
    EXPECT_EQ(0, strcmp("CopyImage3dToBuffer", convertToPascalCase("copy_image3d_to_buffer").c_str()));
    EXPECT_EQ(0, strcmp("CopyImageToImage1d", convertToPascalCase("copy_image_to_image1d").c_str()));
    EXPECT_EQ(0, strcmp("CopyImageToImage2d", convertToPascalCase("copy_image_to_image2d").c_str()));
    EXPECT_EQ(0, strcmp("CopyImageToImage3d", convertToPascalCase("copy_image_to_image3d").c_str()));
    EXPECT_EQ(0, strcmp("FillImage1d", convertToPascalCase("fill_image1d").c_str()));
    EXPECT_EQ(0, strcmp("FillImage2d", convertToPascalCase("fill_image2d").c_str()));
    EXPECT_EQ(0, strcmp("FillImage3d", convertToPascalCase("fill_image3d").c_str()));
    EXPECT_EQ(0, strcmp("VmeBlockMotionEstimateIntel", convertToPascalCase("vme_block_motion_estimate_intel").c_str()));
    EXPECT_EQ(0, strcmp("VmeBlockAdvancedMotionEstimateCheckIntel", convertToPascalCase("vme_block_advanced_motion_estimate_check_intel").c_str()));
    EXPECT_EQ(0, strcmp("VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel", convertToPascalCase("vme_block_advanced_motion_estimate_bidirectional_check_intel").c_str()));
    EXPECT_EQ(0, strcmp("Scheduler", convertToPascalCase("scheduler").c_str()));
    EXPECT_EQ(0, strcmp("", convertToPascalCase("").c_str()));
}

TEST(OfflineCompilerTest, GivenValidParamWhenGettingHardwareInfoThenSuccessIsReturned) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    EXPECT_EQ(CL_INVALID_DEVICE, mockOfflineCompiler->initHardwareInfo("invalid"));
    EXPECT_EQ(PRODUCT_FAMILY::IGFX_UNKNOWN, mockOfflineCompiler->getHardwareInfo().platform.eProductFamily);
    EXPECT_EQ(CL_SUCCESS, mockOfflineCompiler->initHardwareInfo(gEnvironment->devicePrefix.c_str()));
    EXPECT_NE(PRODUCT_FAMILY::IGFX_UNKNOWN, mockOfflineCompiler->getHardwareInfo().platform.eProductFamily);
}

TEST(OfflineCompilerTest, WhenStoringBinaryThenStoredCorrectly) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    const char pSrcBinary[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    const size_t srcBinarySize = sizeof(pSrcBinary);
    char *pDstBinary = new char[srcBinarySize];
    size_t dstBinarySize = srcBinarySize;

    mockOfflineCompiler->storeBinary(pDstBinary, dstBinarySize, pSrcBinary, srcBinarySize);
    EXPECT_EQ(0, memcmp(pDstBinary, pSrcBinary, srcBinarySize));

    delete[] pDstBinary;
}

TEST(OfflineCompilerTest, givenErrorStringsWithoutExtraNullCharactersWhenUpdatingBuildLogThenMessageIsCorrect) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::string errorString = "Error: undefined variable";
    mockOfflineCompiler->updateBuildLog(errorString.c_str(), errorString.length());
    EXPECT_EQ(0, errorString.compare(mockOfflineCompiler->getBuildLog()));

    std::string finalString = "Build failure";
    mockOfflineCompiler->updateBuildLog(finalString.c_str(), finalString.length());
    EXPECT_EQ(0, (errorString + "\n" + finalString).compare(mockOfflineCompiler->getBuildLog().c_str()));
}

TEST(OfflineCompilerTest, givenErrorStringsWithExtraNullCharactersWhenUpdatingBuildLogThenMessageIsCorrect) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::array<char, sizeof("Error: undefined variable\0")> errorMessageArray = {"Error: undefined variable\0"};
    std::string expectedBuildLogString = "Error: undefined variable";
    EXPECT_EQ(errorMessageArray.size(), std::string("Error: undefined variable").length() + 2);

    mockOfflineCompiler->updateBuildLog(errorMessageArray.data(), errorMessageArray.size());
    EXPECT_EQ(mockOfflineCompiler->getBuildLog(), expectedBuildLogString);

    std::array<char, sizeof("Build failure\0")> additionalErrorMessageArray = {"Build failure\0"};
    expectedBuildLogString = "Error: undefined variable\n"
                             "Build failure";
    EXPECT_EQ(additionalErrorMessageArray.size(), std::string("Build failure").length() + 2);

    mockOfflineCompiler->updateBuildLog(additionalErrorMessageArray.data(), additionalErrorMessageArray.size());
    EXPECT_EQ(mockOfflineCompiler->getBuildLog(), expectedBuildLogString);
}

TEST(OfflineCompilerTest, givenValidSizeAndInvalidLogPointerWhenUpdatingBuildLogThenNothingIsWritten) {
    MockOfflineCompiler mockOfflineCompiler{};

    const char *const invalidLog{nullptr};
    const size_t logSize{7};
    mockOfflineCompiler.updateBuildLog(invalidLog, logSize);

    const auto buildLog = mockOfflineCompiler.getBuildLog();
    EXPECT_TRUE(buildLog.empty()) << buildLog;
}

TEST(OfflineCompilerTest, GivenSourceCodeWhenBuildingThenSuccessIsReturned) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, mockOfflineCompiler->genBinary);
    EXPECT_EQ(0u, mockOfflineCompiler->genBinarySize);

    retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, mockOfflineCompiler->genBinary);
    EXPECT_NE(0u, mockOfflineCompiler->genBinarySize);
}

TEST(OfflineCompilerTest, givenSpvOnlyOptionPassedWhenCmdLineParsedThenGenerateOnlySpvFile) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-output",
        "myOutputFileName",
        "-spv_only",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bc") || compilerOutputExists("myOutputFileName", "spv"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));

    retVal = mockOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bc") || compilerOutputExists("myOutputFileName", "spv"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));
}

TEST(OfflineCompilerTest, GivenKernelWhenNoCharAfterKernelSourceThenBuildWithSuccess) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "emptykernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(OfflineCompilerTest, WhenGeneratingElfBinaryThenBinaryIsCreated) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->generateElfBinary();
    EXPECT_FALSE(retVal);

    iOpenCL::SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 3;
    binHeader.Device = mockOfflineCompiler->hwInfo.platform.eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(iOpenCL::SProgramBinaryHeader);

    mockOfflineCompiler->storeGenBinary(&binHeader, binSize);

    EXPECT_TRUE(mockOfflineCompiler->elfBinary.empty());

    retVal = mockOfflineCompiler->generateElfBinary();
    EXPECT_TRUE(retVal);

    EXPECT_FALSE(mockOfflineCompiler->elfBinary.empty());
}

TEST(OfflineCompilerTest, givenInvalidGenBinarySizeAndNotNullPointerWhenGeneratingElfBinaryThenNothingHappensAndFalseIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;

    // Destructor of OfflineCompiler will deallocate the memory.
    mockOfflineCompiler.genBinary = new char[0];
    mockOfflineCompiler.genBinarySize = 0;

    EXPECT_FALSE(mockOfflineCompiler.generateElfBinary());
    EXPECT_TRUE(mockOfflineCompiler.elfBinary.empty());
}

TEST(OfflineCompilerTest, givenNonEmptyOptionsWhenGeneratingElfBinaryThenOptionsSectionIsIncludedInElfAndTrueIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.options = "-some_option";

    // Destructor of OfflineCompiler will deallocate the memory.
    mockOfflineCompiler.genBinary = new char[8]{};
    mockOfflineCompiler.genBinarySize = 8;

    EXPECT_TRUE(mockOfflineCompiler.generateElfBinary());
    ASSERT_FALSE(mockOfflineCompiler.elfBinary.empty());

    std::string errorReason{};
    std::string warning{};

    const auto elf{Elf::decodeElf(mockOfflineCompiler.elfBinary, errorReason, warning)};
    ASSERT_TRUE(errorReason.empty());
    ASSERT_TRUE(warning.empty());

    const auto isOptionsSection = [](const auto &section) {
        return section.header && section.header->type == Elf::SHT_OPENCL_OPTIONS;
    };

    const auto isAnyOptionsSectionDefined = std::any_of(elf.sectionHeaders.begin(), elf.sectionHeaders.end(), isOptionsSection);
    EXPECT_TRUE(isAnyOptionsSectionDefined);
}

TEST(OfflineCompilerTest, givenOutputNoSuffixFlagAndNonEmptyOutputFileNameAndNonEmptyElfContentWhenWritingOutAllFilesThenFileWithCorrectNameIsWritten) {
    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;

    mockOfflineCompiler.outputNoSuffix = true;
    mockOfflineCompiler.outputFile = "some_output_filename";
    mockOfflineCompiler.elfBinary = {49, 50, 51, 52, 53, 54, 55, 56}; // ASCII codes of "12345678"

    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    EXPECT_EQ("12345678", outputFileIt->second);
}

TEST(OfflineCompilerTest, givenInputFileNameAndOutputNoSuffixFlagAndEmptyOutputFileNameAndNonEmptyElfContentWhenWritingOutAllFilesThenFileWithTruncatedInputNameIsWritten) {
    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;

    mockOfflineCompiler.outputNoSuffix = true;
    mockOfflineCompiler.inputFile = "/home/important_file.spv";
    mockOfflineCompiler.elfBinary = {49, 50, 51, 52, 53, 54, 55, 56}; // ASCII codes of "12345678"

    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("important_file");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    EXPECT_EQ("12345678", outputFileIt->second);
}

TEST(OfflineCompilerTest, givenNonEmptyOutputDirectoryWhenWritingOutAllFilesTheDirectoriesAreCreatedAndFileIsWrittenToIt) {
    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.interceptCreatedDirs = true;
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;

    mockOfflineCompiler.outputDirectory = "/home/important/compilation";
    mockOfflineCompiler.outputNoSuffix = true;
    mockOfflineCompiler.outputFile = "some_output_filename";
    mockOfflineCompiler.elfBinary = {49, 50, 51, 52, 53, 54, 55, 56}; // ASCII codes of "12345678"

    mockOfflineCompiler.writeOutAllFiles();

    ASSERT_EQ(3u, mockOfflineCompiler.createdDirs.size());

    EXPECT_EQ("/home", mockOfflineCompiler.createdDirs[0]);
    EXPECT_EQ("/home/important", mockOfflineCompiler.createdDirs[1]);
    EXPECT_EQ("/home/important/compilation", mockOfflineCompiler.createdDirs[2]);

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("/home/important/compilation/some_output_filename");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    EXPECT_EQ("12345678", outputFileIt->second);
}

TEST(OfflineCompilerTest, givenLlvmInputOptionPassedWhenCmdLineParsedThenInputFileLlvmIsSetTrue) {
    std::vector<std::string> argv = {
        "ocloc",
        "-llvm_input"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    testing::internal::CaptureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(0u, output.size());

    bool llvmFileOption = mockOfflineCompiler->inputFileLlvm;
    EXPECT_TRUE(llvmFileOption);
}

TEST(OfflineCompilerTest, givenDefaultOfflineCompilerObjectWhenNoOptionsAreChangedThenLlvmInputFileIsFalse) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    bool llvmFileOption = mockOfflineCompiler->inputFileLlvm;
    EXPECT_FALSE(llvmFileOption);
}

TEST(OfflineCompilerTest, givenSpirvInputOptionPassedWhenCmdLineParsedThenInputFileSpirvIsSetTrue) {
    std::vector<std::string> argv = {"ocloc", "-spirv_input"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());

    testing::internal::CaptureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(0u, output.size());

    EXPECT_TRUE(mockOfflineCompiler->inputFileSpirV);
}

TEST(OfflineCompilerTest, givenDefaultOfflineCompilerObjectWhenNoOptionsAreChangedThenSpirvInputFileIsFalse) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    EXPECT_FALSE(mockOfflineCompiler->inputFileSpirV);
}

TEST(OfflineCompilerTest, givenIntermediateRepresentationInputWhenBuildSourceCodeIsCalledThenProperTranslationContextIsUsed) {
    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "emptykernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    testing::internal::CaptureStdout();
    auto retVal = mockOfflineCompiler.initialize(argv.size(), argv);
    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.mockIgcFacade->igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxTagOCL>(mockIgcOclDeviceCtx);
    ASSERT_EQ(CL_SUCCESS, retVal);

    mockOfflineCompiler.inputFileSpirV = true;
    retVal = mockOfflineCompiler.buildSourceCode();
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1U, mockIgcOclDeviceCtx->requestedTranslationCtxs.size());
    NEO::MockIgcOclDeviceCtx::TranslationOpT expectedTranslation = {IGC::CodeType::spirV, IGC::CodeType::oclGenBin};
    ASSERT_EQ(expectedTranslation, mockIgcOclDeviceCtx->requestedTranslationCtxs[0]);

    mockOfflineCompiler.inputFileSpirV = false;
    mockOfflineCompiler.inputFileLlvm = true;
    mockIgcOclDeviceCtx->requestedTranslationCtxs.clear();
    retVal = mockOfflineCompiler.buildSourceCode();

    ASSERT_EQ(mockOfflineCompiler.irBinarySize, mockOfflineCompiler.sourceCode.size());
    EXPECT_EQ(0, memcmp(mockOfflineCompiler.irBinary, mockOfflineCompiler.sourceCode.data(), mockOfflineCompiler.sourceCode.size()));
    EXPECT_FALSE(mockOfflineCompiler.isSpirV);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1U, mockIgcOclDeviceCtx->requestedTranslationCtxs.size());
    expectedTranslation = {IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin};
    ASSERT_EQ(expectedTranslation, mockIgcOclDeviceCtx->requestedTranslationCtxs[0]);

    testing::internal::GetCapturedStdout();
}

TEST(OfflineCompilerTest, givenUseLlvmBcFlagWhenBuildingIrBinaryThenProperTranslationContextIsUsed) {
    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "emptykernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    ASSERT_EQ(CL_SUCCESS, initResult);

    auto mockFclOclDeviceCtx = new NEO::MockFclOclDeviceCtx();
    mockOfflineCompiler.mockFclFacade->fclDeviceCtx = CIF::RAII::Pack<IGC::FclOclDeviceCtxTagOCL>(mockFclOclDeviceCtx);

    mockOfflineCompiler.useLlvmBc = true;
    const auto buildResult = mockOfflineCompiler.buildIrBinary();
    EXPECT_EQ(CL_SUCCESS, buildResult);

    ASSERT_EQ(1U, mockFclOclDeviceCtx->requestedTranslationCtxs.size());

    NEO::MockIgcOclDeviceCtx::TranslationOpT expectedTranslation = {IGC::CodeType::oclC, IGC::CodeType::llvmBc};
    EXPECT_EQ(expectedTranslation, mockFclOclDeviceCtx->requestedTranslationCtxs[0]);
}

TEST(OfflineCompilerTest, givenBinaryInputThenDontTruncateSourceAtFirstZero) {
    std::vector<std::string> argvLlvm = {"ocloc", "-llvm_input", "-file", clFiles + "binary_with_zeroes",
                                         "-device", gEnvironment->devicePrefix.c_str()};
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    mockOfflineCompiler->initialize(argvLlvm.size(), argvLlvm);
    EXPECT_LT(0U, mockOfflineCompiler->sourceCode.size());

    std::vector<std::string> argvSpirV = {"ocloc", "-spirv_input", "-file", clFiles + "binary_with_zeroes",
                                          "-device", gEnvironment->devicePrefix.c_str()};
    mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    mockOfflineCompiler->initialize(argvSpirV.size(), argvSpirV);
    EXPECT_LT(0U, mockOfflineCompiler->sourceCode.size());
}

TEST(OfflineCompilerTest, givenSpirvInputFileWhenCmdLineHasOptionsThenCorrectOptionsArePassedToCompiler) {
    char data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    MockCompilerDebugVars igcDebugVars(gEnvironment->igcDebugVars);
    igcDebugVars.binaryToReturn = data;
    igcDebugVars.binaryToReturnSize = sizeof(data);

    NEO::setIgcDebugVars(igcDebugVars);

    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "emptykernel.cl",
        "-spirv_input",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "test_options_passed"};

    auto retVal = mockOfflineCompiler.initialize(argv.size(), argv);
    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.mockIgcFacade->igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxTagOCL>(mockIgcOclDeviceCtx);
    ASSERT_EQ(CL_SUCCESS, retVal);

    mockOfflineCompiler.inputFileSpirV = true;

    retVal = mockOfflineCompiler.buildSourceCode();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_STREQ("test_options_passed", mockOfflineCompiler.options.c_str());
    NEO::setIgcDebugVars(gEnvironment->igcDebugVars);
}

TEST(OfflineCompilerTest, givenOutputFileOptionWhenSourceIsCompiledThenOutputFileHasCorrectName) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-output",
        "myOutputFileName",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bc") || compilerOutputExists("myOutputFileName", "spv"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));

    retVal = mockOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bc") || compilerOutputExists("myOutputFileName", "spv"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "gen"));
}

TEST(OfflineCompilerTest, givenDebugDataAvailableWhenSourceIsBuiltThenDebugDataFileIsCreated) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-output",
        "myOutputFileName",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    char debugData[10];
    MockCompilerDebugVars igcDebugVars(gEnvironment->igcDebugVars);
    igcDebugVars.debugDataToReturn = debugData;
    igcDebugVars.debugDataToReturnSize = sizeof(debugData);

    NEO::setIgcDebugVars(igcDebugVars);

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bc") || compilerOutputExists("myOutputFileName", "spv"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "dbg"));

    retVal = mockOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bc") || compilerOutputExists("myOutputFileName", "spv"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "gen"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "dbg"));

    NEO::setIgcDebugVars(gEnvironment->igcDebugVars);
}

TEST(OfflineCompilerTest, givenInternalOptionsWhenCmdLineParsedThenOptionsAreAppendedToInternalOptionsString) {
    std::vector<std::string> argv = {
        "ocloc",
        "-internal_options",
        "myInternalOptions"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    testing::internal::CaptureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(0u, output.size());

    std::string internalOptions = mockOfflineCompiler->internalOptions;
    EXPECT_TRUE(hasSubstr(internalOptions, std::string("myInternalOptions")));
}

TEST(OfflineCompilerTest, givenInputOptionsAndInternalOptionsFilesWhenOfflineCompilerIsInitializedThenCorrectOptionsAreSetAndRemainAfterBuild) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    ASSERT_TRUE(fileExists(clFiles + "shouldfail_options.txt"));
    ASSERT_TRUE(fileExists(clFiles + "shouldfail_internal_options.txt"));

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "shouldfail.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &options = mockOfflineCompiler->options;
    auto &internalOptions = mockOfflineCompiler->internalOptions;
    EXPECT_STREQ(options.c_str(), "-shouldfailOptions");
    EXPECT_TRUE(internalOptions.find("-shouldfailInternalOptions") != std::string::npos);
    EXPECT_TRUE(mockOfflineCompiler->getOptionsReadFromFile().find("-shouldfailOptions") != std::string::npos);
    EXPECT_TRUE(mockOfflineCompiler->getInternalOptionsReadFromFile().find("-shouldfailInternalOptions") != std::string::npos);

    mockOfflineCompiler->build();

    EXPECT_STREQ(options.c_str(), "-shouldfailOptions");
    EXPECT_TRUE(internalOptions.find("-shouldfailInternalOptions") != std::string::npos);
    EXPECT_TRUE(mockOfflineCompiler->getOptionsReadFromFile().find("-shouldfailOptions") != std::string::npos);
    EXPECT_TRUE(mockOfflineCompiler->getInternalOptionsReadFromFile().find("-shouldfailInternalOptions") != std::string::npos);
}

TEST(OfflineCompilerTest, givenInputOptionsFileWithSpecialCharsWhenOfflineCompilerIsInitializedThenCorrectOptionsAreSet) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    ASSERT_TRUE(fileExists(clFiles + "simple_kernels_opts_options.txt"));

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "simple_kernels_opts.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &options = mockOfflineCompiler->options;
    EXPECT_STREQ(options.c_str(), "-cl-opt-disable -DDEF_WAS_SPECIFIED=1 -DARGS=\", const __global int *arg1, float arg2, const __global int *arg3, float arg4\"");
}

TEST(OfflineCompilerTest, givenInputOptionsAndOclockOptionsFileWithForceStosOptWhenOfflineCompilerIsInitializedThenCompilerOptionGreaterThan4gbBuffersRequiredIsNotApplied) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    ASSERT_TRUE(fileExists(clFiles + "stateful_copy_buffer_ocloc_options.txt"));

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "stateful_copy_buffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    mockOfflineCompiler->build();

    auto &internalOptions = mockOfflineCompiler->internalOptions;
    size_t found = internalOptions.find(NEO::CompilerOptions::greaterThan4gbBuffersRequired.data());
    EXPECT_EQ(std::string::npos, found);
}

struct OfflineCompilerStatelessToStatefulTests : public ::testing::Test {
    void SetUp() override {
        mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
        mockOfflineCompiler->deviceName = gEnvironment->devicePrefix;
        mockOfflineCompiler->initHardwareInfo(mockOfflineCompiler->deviceName);
    }
    void runTest() const {
        std::pair<bool, bool> testParams[] = {{true, false}, {false, true}};

        for (const auto &[forceStatelessToStatefulOptimization, containsGreaterThan4gbBuffersRequired] : testParams) {
            auto internalOptions = mockOfflineCompiler->internalOptions;
            mockOfflineCompiler->forceStatelessToStatefulOptimization = forceStatelessToStatefulOptimization;
            mockOfflineCompiler->appendExtraInternalOptions(internalOptions);
            auto found = internalOptions.find(NEO::CompilerOptions::greaterThan4gbBuffersRequired.data());

            if (containsGreaterThan4gbBuffersRequired) {
                EXPECT_NE(std::string::npos, found);
            } else {
                EXPECT_EQ(std::string::npos, found);
            }
        }
    }

    std::unique_ptr<MockOfflineCompiler> mockOfflineCompiler;
};

TEST_F(OfflineCompilerStatelessToStatefulTests, whenAppendExtraInternalOptionsThenInternalOptionsAreCorrect) {
    const auto &compilerHwInfoConfig = *CompilerHwInfoConfig::get(mockOfflineCompiler->hwInfo.platform.eProductFamily);
    if (!compilerHwInfoConfig.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }
    runTest();
}

template <PRODUCT_FAMILY productFamily>
class MockCompilerHwInfoConfigHw : public CompilerHwInfoConfigHw<productFamily> {
  public:
    bool isForceToStatelessRequired() const override {
        return true;
    }
};

HWTEST2_F(OfflineCompilerStatelessToStatefulTests, givenMockWhenAppendExtraInternalOptionsThenInternalOptionsAreCorrect, MatchAny) {
    MockCompilerHwInfoConfigHw<productFamily> mockCompilerHwInfoConfig;
    VariableBackup<CompilerHwInfoConfig *> backupMockHwInfoConfig(&CompilerHwInfoConfigFactory[productFamily], &mockCompilerHwInfoConfig);
    runTest();
}

TEST(OfflineCompilerTest, givenNonExistingFilenameWhenUsedToReadOptionsThenReadOptionsFromFileReturnsFalse) {
    std::string options;
    std::string file("non_existing_file");
    ASSERT_FALSE(fileExists(file.c_str()));

    auto helper = std::make_unique<OclocArgHelper>();
    bool result = OfflineCompiler::readOptionsFromFile(options, file, helper.get());

    EXPECT_FALSE(result);
}

class MyOclocArgHelper : public OclocArgHelper {
  public:
    std::unique_ptr<char[]> loadDataFromFile(const std::string &filename, size_t &retSize) override {
        auto file = std::make_unique<char[]>(fileContent.size() + 1);
        strcpy_s(file.get(), fileContent.size() + 1, fileContent.c_str());
        retSize = fileContent.size();
        return file;
    }

    bool fileExists(const std::string &filename) const override {
        return true;
    }

    std::string fileContent;
};

TEST(OfflineCompilerTest, givenEmptyFileWhenReadOptionsFromFileThenSuccessIsReturned) {
    std::string options;
    std::string filename("non_existing_file");

    auto helper = std::make_unique<MyOclocArgHelper>();
    helper->fileContent = "";

    EXPECT_TRUE(OfflineCompiler::readOptionsFromFile(options, filename, helper.get()));
    EXPECT_TRUE(options.empty());
}

TEST(OfflineCompilerTest, givenNoCopyrightsWhenReadOptionsFromFileThenSuccessIsReturned) {
    std::string options;
    std::string filename("non_existing_file");

    auto helper = std::make_unique<MyOclocArgHelper>();
    helper->fileContent = "-dummy_option";
    EXPECT_TRUE(OfflineCompiler::readOptionsFromFile(options, filename, helper.get()));
    EXPECT_STREQ(helper->fileContent.c_str(), options.c_str());
}

TEST(OfflineCompilerTest, givenEmptyDirectoryWhenGenerateFilePathIsCalledThenTrailingSlashIsNotAppended) {
    std::string path = generateFilePath("", "a", "b");
    EXPECT_STREQ("ab", path.c_str());
}

TEST(OfflineCompilerTest, givenNonEmptyDirectoryWithTrailingSlashWhenGenerateFilePathIsCalledThenAdditionalTrailingSlashIsNotAppended) {
    std::string path = generateFilePath("d/", "a", "b");
    EXPECT_STREQ("d/ab", path.c_str());
}

TEST(OfflineCompilerTest, givenNonEmptyDirectoryWithoutTrailingSlashWhenGenerateFilePathIsCalledThenTrailingSlashIsAppended) {
    std::string path = generateFilePath("d", "a", "b");
    EXPECT_STREQ("d/ab", path.c_str());
}

TEST(OfflineCompilerTest, givenSpirvPathWhenGenerateFilePathForIrIsCalledThenProperExtensionIsReturned) {
    MockOfflineCompiler compiler;
    compiler.isSpirV = true;
    compiler.outputDirectory = "d";
    std::string path = compiler.generateFilePathForIr("a");
    EXPECT_STREQ("d/a.spv", path.c_str());
}

TEST(OfflineCompilerTest, givenLlvmBcPathWhenGenerateFilePathForIrIsCalledThenProperExtensionIsReturned) {
    MockOfflineCompiler compiler;
    compiler.isSpirV = false;
    compiler.outputDirectory = "d";
    std::string path = compiler.generateFilePathForIr("a");
    EXPECT_STREQ("d/a.bc", path.c_str());
}

TEST(OfflineCompilerTest, givenLlvmTextPathWhenGenerateFilePathForIrIsCalledThenProperExtensionIsReturned) {
    MockOfflineCompiler compiler;
    compiler.isSpirV = false;
    compiler.useLlvmText = true;
    compiler.outputDirectory = "d";
    std::string path = compiler.generateFilePathForIr("a");
    EXPECT_STREQ("d/a.ll", path.c_str());

    compiler.isSpirV = true;
    path = compiler.generateFilePathForIr("a");
    EXPECT_STREQ("d/a.ll", path.c_str());
}

TEST(OfflineCompilerTest, givenDisabledOptsSuffixWhenGenerateOptsSuffixIsCalledThenEmptyStringIsReturned) {
    MockOfflineCompiler compiler;
    compiler.options = "A B C";
    compiler.useOptionsSuffix = false;
    std::string suffix = compiler.generateOptsSuffix();
    EXPECT_STREQ("", suffix.c_str());
}

TEST(OfflineCompilerTest, givenEnabledOptsSuffixWhenGenerateOptsSuffixIsCalledThenEscapedStringIsReturned) {
    MockOfflineCompiler compiler;
    compiler.options = "A B C";
    compiler.useOptionsSuffix = true;
    std::string suffix = compiler.generateOptsSuffix();
    EXPECT_STREQ("A_B_C", suffix.c_str());
}

TEST(OfflineCompilerTest, givenCompilerWhenBuildSourceCodeFailsThenGenerateElfBinaryAndWriteOutAllFilesAreCalled) {
    MockOfflineCompiler compiler;
    compiler.overrideBuildSourceCodeStatus = true;

    auto expectedError = OclocErrorCode::BUILD_PROGRAM_FAILURE;
    compiler.buildSourceCodeStatus = expectedError;

    EXPECT_EQ(0u, compiler.generateElfBinaryCalled);
    EXPECT_EQ(0u, compiler.writeOutAllFilesCalled);

    auto status = compiler.build();
    EXPECT_EQ(expectedError, status);

    EXPECT_EQ(1u, compiler.generateElfBinaryCalled);
    EXPECT_EQ(1u, compiler.writeOutAllFilesCalled);
}

TEST(OfflineCompilerTest, givenDeviceSpecificKernelFileWhenCompilerIsInitializedThenOptionsAreReadFromFile) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    std::string kernelFileName(clFiles + "kernel_for_specific_device.skl");
    std::string optionsFileName(clFiles + "kernel_for_specific_device_options.txt");

    ASSERT_TRUE(fileExists(kernelFileName));
    ASSERT_TRUE(fileExists(optionsFileName));

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        kernelFileName,
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    EXPECT_STREQ("-cl-opt-disable", mockOfflineCompiler->options.c_str());
}

TEST(OfflineCompilerTest, givenHexadecimalRevisionIdWhenCompilerIsInitializedThenPassItToHwInfo) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-revision_id",
        "0x11"};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, 17);
}

TEST(OfflineCompilerTest, givenDebugVariableSetWhenInitializingThenOverrideRevision) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideRevision.set(123);

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-revision_id",
        "0x11"};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, 123);
}

TEST(OfflineCompilerTest, givenDecimalRevisionIdWhenCompilerIsInitializedThenPassItToHwInfo) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-revision_id",
        "17"};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, 17);
}

TEST(OfflineCompilerTest, givenNoRevisionIdWhenCompilerIsInitializedThenHwInfoHasDefaultRevId) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    mockOfflineCompiler->initHardwareInfo(gEnvironment->devicePrefix.c_str());
    auto revId = mockOfflineCompiler->hwInfo.platform.usRevId;
    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, revId);
}

TEST(OfflineCompilerTest, givenEmptyDeviceNameWhenInitializingHardwareInfoThenInvalidDeviceIsReturned) {
    MockOfflineCompiler mockOfflineCompiler{};

    const std::string emptyDeviceName{};
    const auto result = mockOfflineCompiler.initHardwareInfo(emptyDeviceName);

    EXPECT_EQ(OclocErrorCode::INVALID_DEVICE, result);
}

TEST(OfflineCompilerTest, whenDeviceIsSpecifiedThenDefaultConfigFromTheDeviceIsUsed) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::SUCCESS, retVal);

    HardwareInfo hwInfo = mockOfflineCompiler->hwInfo;

    uint32_t sliceCount = 2;
    uint32_t subSlicePerSliceCount = 4;
    uint32_t euPerSubSliceCount = 5;

    uint64_t hwInfoConfig = euPerSubSliceCount;
    hwInfoConfig |= (static_cast<uint64_t>(subSlicePerSliceCount) << 16);
    hwInfoConfig |= (static_cast<uint64_t>(sliceCount) << 32);

    setHwInfoValuesFromConfig(hwInfoConfig, hwInfo);

    EXPECT_EQ(sliceCount, hwInfo.gtSystemInfo.SliceCount);
    EXPECT_EQ(subSlicePerSliceCount * sliceCount, hwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(subSlicePerSliceCount * sliceCount, hwInfo.gtSystemInfo.SubSliceCount);
    EXPECT_EQ(euPerSubSliceCount * subSlicePerSliceCount * sliceCount, hwInfo.gtSystemInfo.EUCount);
}

TEST(OclocCompile, whenDetectedPotentialInputTypeMismatchThenEmitsWarning) {
    std::string sourceOclC = "__kernel void k() { }";
    std::string sourceLlvmBc = NEO::llvmBcMagic.str();
    std::string sourceSpirv = NEO::spirvMagic.str();
    std::string sourceSpirvInv = NEO::spirvMagicInv.str();

    std::string notSpirvWarning = "Warning : file does not look like spirv bitcode (wrong magic numbers)";
    std::string notLlvmBcWarning = "Warning : file does not look like llvm bitcode (wrong magic numbers)";
    std::string isSpirvWarning = "Warning : file looks like spirv bitcode (based on magic numbers) - please make sure proper CLI flags are present";
    std::string isLlvmBcWarning = "Warning : file looks like llvm bitcode (based on magic numbers) - please make sure proper CLI flags are present";
    std::string allWarnings[] = {notSpirvWarning, notLlvmBcWarning, isLlvmBcWarning, isSpirvWarning};

    struct Case {
        std::string input;
        bool isSpirv;
        bool isLlvm;
        std::string expectedWarning;
    };

    Case cases[] = {
        {sourceOclC, false, false, ""},
        {sourceOclC, true, false, notSpirvWarning},
        {sourceOclC, false, true, notLlvmBcWarning},

        {sourceLlvmBc, false, false, isLlvmBcWarning},
        {sourceLlvmBc, true, false, notSpirvWarning},
        {sourceLlvmBc, false, true, ""},

        {sourceSpirv, false, false, isSpirvWarning},
        {sourceSpirv, true, false, ""},
        {sourceSpirv, false, true, notLlvmBcWarning},

        {sourceSpirvInv, false, false, isSpirvWarning},
        {sourceSpirvInv, true, false, ""},
        {sourceSpirvInv, false, true, notLlvmBcWarning},
    };
    {
        MockOfflineCompiler ocloc;

        std::vector<std::string> argv = {
            "ocloc",
            "-q",
            "-file",
            clFiles + "copybuffer.cl",
            "-device",
            gEnvironment->devicePrefix.c_str()};

        ocloc.initHardwareInfo(gEnvironment->devicePrefix.c_str());
        int retVal = ocloc.initialize(argv.size(), argv);
        ASSERT_EQ(0, retVal);

        int caseNum = 0;
        for (auto &c : cases) {

            testing::internal::CaptureStdout();

            ocloc.sourceCode = c.input;
            ocloc.inputFileLlvm = c.isLlvm;
            ocloc.inputFileSpirV = c.isSpirv;
            ocloc.build();
            auto log = ocloc.argHelper->getPrinterRef().getLog().str();
            ocloc.clearLog();

            std::string output = testing::internal::GetCapturedStdout();

            if (c.expectedWarning.empty()) {
                for (auto &w : allWarnings) {
                    EXPECT_FALSE(hasSubstr(log, w)) << " Case : " << caseNum;
                }
            } else {
                EXPECT_TRUE(hasSubstr(log, c.expectedWarning)) << " Case : " << caseNum;
                EXPECT_STREQ(log.c_str(), output.c_str());
            }
            caseNum++;
        }
    }
}

TEST(OclocCompile, givenCommandLineWithoutDeviceWhenCompilingToSpirvThenSucceedsButUsesEmptyExtensionString) {
    MockOfflineCompiler ocloc;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-spv_only"};

    int retVal = ocloc.initialize(argv.size(), argv);
    ASSERT_EQ(0, retVal);
    retVal = ocloc.build();
    EXPECT_EQ(0, retVal);
    EXPECT_TRUE(hasSubstr(ocloc.internalOptions, "-ocl-version=300 -cl-ext=-all,+cl_khr_3d_image_writes -D__IMAGE_SUPPORT__=1"));
}

TEST(OclocCompile, givenDeviceAndInternalOptionsOptionWhenCompilingToSpirvThenInternalOptionsAreSetCorrectly) {
    MockOfflineCompiler ocloc;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-internal_options",
        "-cl-ext=+custom_param",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spv_only"};

    int retVal = ocloc.initialize(argv.size(), argv);
    ASSERT_EQ(0, retVal);
    retVal = ocloc.build();
    EXPECT_EQ(0, retVal);
    std::string regexToMatch = "\\-ocl\\-version=" + std::to_string(ocloc.hwInfo.capabilityTable.clVersionSupport) +
                               "0  \\-cl\\-ext=\\-all.* \\-cl\\-ext=\\+custom_param";
    EXPECT_TRUE(containsRegex(ocloc.internalOptions, regexToMatch));
}

TEST(OclocCompile, givenNoDeviceAndInternalOptionsOptionWhenCompilingToSpirvThenInternalOptionsAreSetCorrectly) {
    MockOfflineCompiler ocloc;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-internal_options",
        "-cl-ext=+custom_param",
        "-spv_only"};

    int retVal = ocloc.initialize(argv.size(), argv);
    ASSERT_EQ(0, retVal);
    retVal = ocloc.build();
    EXPECT_EQ(0, retVal);
    EXPECT_TRUE(hasSubstr(ocloc.internalOptions, "-ocl-version=300 -cl-ext=-all,+cl_khr_3d_image_writes -cl-ext=+custom_param"));
}
TEST(OclocCompile, givenPackedDeviceBinaryFormatWhenGeneratingElfBinaryThenItIsReturnedAsItIs) {
    MockOfflineCompiler ocloc;
    ZebinTestData::ValidEmptyProgram zebin;

    // genBinary is deleted in ocloc's destructor
    ocloc.genBinary = new char[zebin.storage.size()];
    ocloc.genBinarySize = zebin.storage.size();
    memcpy_s(ocloc.genBinary, ocloc.genBinarySize, zebin.storage.data(), zebin.storage.size());

    ASSERT_EQ(true, ocloc.generateElfBinary());
    EXPECT_EQ(0, memcmp(zebin.storage.data(), ocloc.elfBinary.data(), zebin.storage.size()));
}

TEST(OclocCompile, givenSpirvInputThenDontGenerateSpirvFile) {
    MockOfflineCompiler ocloc;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "binary_with_zeroes",
        "-out_dir",
        "offline_compiler_test",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spirv_input"};

    int retVal = ocloc.initialize(argv.size(), argv);
    ASSERT_EQ(0, retVal);
    retVal = ocloc.build();
    EXPECT_EQ(0, retVal);
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/binary_with_zeroes", "gen"));
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/binary_with_zeroes", "bin"));
    EXPECT_FALSE(compilerOutputExists("offline_compiler_test/binary_with_zeroes", "spv"));
}

TEST(OclocCompile, givenFormatFlagWithKnownFormatPassedThenEnforceSpecifiedFormatAccordingly) {
    MockOfflineCompiler ocloc;

    std::vector<std::string> argvEnforcedFormatPatchtokens = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-spv_only",
        "--format",
        "patchtokens"};

    std::vector<std::string> argvEnforcedFormatZebin = {
        "ocloc",
        "-q",
        "-file",
        clFiles + "copybuffer.cl",
        "-spv_only",
        "--format",
        "zebin"};

    int retVal = ocloc.initialize(argvEnforcedFormatZebin.size(), argvEnforcedFormatZebin);
    ASSERT_EQ(0, retVal);
    EXPECT_TRUE(hasSubstr(ocloc.internalOptions, std::string{CompilerOptions::allowZebin}));

    ocloc.internalOptions.clear();
    retVal = ocloc.initialize(argvEnforcedFormatPatchtokens.size(), argvEnforcedFormatPatchtokens);
    ASSERT_EQ(0, retVal);
    EXPECT_FALSE(hasSubstr(ocloc.internalOptions, std::string{CompilerOptions::allowZebin}));
}

TEST(OclocCompile, givenFormatFlagWithUnknownFormatPassedThenPrintWarning) {
    MockOfflineCompiler ocloc;

    std::vector<std::string> argvUnknownFormatEnforced = {
        "ocloc",
        "-file",
        clFiles + "copybuffer.cl",
        "-spv_only",
        "--format",
        "banana"};

    ::testing::internal::CaptureStdout();
    int retVal = ocloc.initialize(argvUnknownFormatEnforced.size(), argvUnknownFormatEnforced);
    const auto output = testing::internal::GetCapturedStdout();
    ASSERT_EQ(0, retVal);

    const auto expectedOutput{"Invalid format passed: banana. Ignoring.\n"};
    EXPECT_EQ(expectedOutput, output);
}

TEST(OfflineCompilerTest, GivenDebugFlagWhenSetStatelessToStatefulBufferOffsetFlagThenStatelessToStatefullOptimizationIsSetCorrectly) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    {
        DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(0);
        mockOfflineCompiler.initHardwareInfo(gEnvironment->devicePrefix.c_str());
        mockOfflineCompiler.setStatelessToStatefulBufferOffsetFlag();
        std::string internalOptions = mockOfflineCompiler.internalOptions;
        size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
        EXPECT_EQ(std::string::npos, found);
    }
    {
        DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);
        mockOfflineCompiler.initHardwareInfo(gEnvironment->devicePrefix.c_str());
        mockOfflineCompiler.setStatelessToStatefulBufferOffsetFlag();
        std::string internalOptions = mockOfflineCompiler.internalOptions;
        size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
        EXPECT_NE(std::string::npos, found);
    }
}

struct WhiteBoxOclocArgHelper : public OclocArgHelper {
    using OclocArgHelper::messagePrinter;
    using OclocArgHelper::OclocArgHelper;
};

TEST(OclocArgHelperTest, GivenOutputSuppressMessagesAndSaveItToFile) {
    uint32_t numOutputs = 0U;
    uint64_t *lenOutputs = nullptr;
    uint8_t **outputs = nullptr;
    char **nameOutputs = nullptr;
    auto helper = std::unique_ptr<WhiteBoxOclocArgHelper>(new WhiteBoxOclocArgHelper(0, nullptr, nullptr, nullptr,
                                                                                     0, nullptr, nullptr, nullptr,
                                                                                     &numOutputs, &outputs, &lenOutputs, &nameOutputs));
    EXPECT_TRUE(helper->messagePrinter.isSuppressed());

    ConstStringRef printMsg = "Hello world!";
    testing::internal::CaptureStdout();
    helper->printf(printMsg.data());
    std::string capturedStdout = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(capturedStdout.empty());

    helper.reset(); // Delete helper. Destructor saves data to output
    EXPECT_EQ(1U, numOutputs);
    EXPECT_EQ(printMsg.length(), lenOutputs[0]);
    EXPECT_STREQ("stdout.log", nameOutputs[0]);
    std::string stdoutStr = std::string(reinterpret_cast<const char *>(outputs[0]),
                                        static_cast<size_t>(lenOutputs[0]));
    EXPECT_STREQ(printMsg.data(), stdoutStr.c_str());

    delete[] nameOutputs[0];
    delete[] outputs[0];
    delete[] nameOutputs;
    delete[] outputs;
    delete[] lenOutputs;
}

TEST(OclocArgHelperTest, GivenValidSourceFileWhenRequestingVectorOfStringsThenLinesAreStored) {
    const char input[] = "First\nSecond\nThird";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    std::vector<std::string> lines{};
    source.toVectorOfStrings(lines);

    ASSERT_EQ(3u, lines.size());

    EXPECT_EQ("First", lines[0]);
    EXPECT_EQ("Second", lines[1]);
    EXPECT_EQ("Third", lines[2]);
}

TEST(OclocArgHelperTest, GivenSourceFileWithTabsWhenRequestingVectorOfStringsWithTabsReplacementThenLinesWithSpacesAreStored) {
    const char input[] = "First\tWord\nSecond\tWord\nThird\tWord";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    constexpr bool replaceTabs{true};
    std::vector<std::string> lines{};
    source.toVectorOfStrings(lines, replaceTabs);

    ASSERT_EQ(3u, lines.size());

    EXPECT_EQ("First Word", lines[0]);
    EXPECT_EQ("Second Word", lines[1]);
    EXPECT_EQ("Third Word", lines[2]);
}

TEST(OclocArgHelperTest, GivenSourceFileWithEmptyLinesWhenRequestingVectorOfStringsThenOnlyNonEmptyLinesAreStored) {
    const char input[] = "First\n\n\nSecond\n";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    std::vector<std::string> lines{};
    source.toVectorOfStrings(lines);

    ASSERT_EQ(2u, lines.size());

    EXPECT_EQ("First", lines[0]);
    EXPECT_EQ("Second", lines[1]);
}

TEST(OclocArgHelperTest, GivenSourceFileWhenRequestingBinaryVectorThenBinaryIsReturned) {
    const char input[] = "A file content";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};
    const auto binaryContent = source.toBinaryVector();

    ASSERT_EQ(inputLength, binaryContent.size());
    ASSERT_TRUE(std::equal(binaryContent.begin(), binaryContent.end(), input));
}

TEST(OclocArgHelperTest, GivenNoOutputPrintMessages) {
    auto helper = WhiteBoxOclocArgHelper(0, nullptr, nullptr, nullptr,
                                         0, nullptr, nullptr, nullptr,
                                         nullptr, nullptr, nullptr, nullptr);
    EXPECT_FALSE(helper.messagePrinter.isSuppressed());
    ConstStringRef printMsg = "Hello world!";
    testing::internal::CaptureStdout();
    helper.printf(printMsg.data());
    std::string capturedStdout = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(printMsg.data(), capturedStdout.c_str());
}

TEST(OclocArgHelperTest, GivenValidHeaderFileWhenRequestingHeadersAsVectorOfStringsThenLinesAreStored) {
    const char input[] = "First\nSecond\nThird";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockOclocArgHelper{mockArgHelperFilesMap};

    mockOclocArgHelper.headers.push_back(source);

    const auto lines = mockOclocArgHelper.headersToVectorOfStrings();
    ASSERT_EQ(3u, lines.size());

    EXPECT_EQ("First", lines[0]);
    EXPECT_EQ("Second", lines[1]);
    EXPECT_EQ("Third", lines[2]);
}

TEST(OclocArgHelperTest, GivenValidSourceFileWhenLookingForItThenItIsReturned) {
    const char input[] = "First\nSecond\nThird";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockOclocArgHelper{mockArgHelperFilesMap};

    mockOclocArgHelper.inputs.push_back(source);

    const auto file = mockOclocArgHelper.findSourceFile("some_file.txt");
    EXPECT_EQ(&mockOclocArgHelper.inputs[0], file);
}

TEST(OclocArgHelperTest, GivenValidSourceFileWhenLookingForAnotherOneThenNullptrIsReturned) {
    const char input[] = "First\nSecond\nThird";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockOclocArgHelper{mockArgHelperFilesMap};

    mockOclocArgHelper.inputs.push_back(source);

    const auto file = mockOclocArgHelper.findSourceFile("another_file.txt");
    EXPECT_EQ(nullptr, file);
}

TEST(OclocArgHelperTest, GivenValidSourceFileWhenReadingItViaHelperAsVectorOfStringsThenLinesAreReturned) {
    const char input[] = "First\nSecond\nThird";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockOclocArgHelper{mockArgHelperFilesMap};

    mockOclocArgHelper.callBaseReadFileToVectorOfStrings = true;
    mockOclocArgHelper.inputs.push_back(source);

    std::vector<std::string> lines{};
    mockOclocArgHelper.readFileToVectorOfStrings("some_file.txt", lines);
    ASSERT_EQ(3u, lines.size());

    EXPECT_EQ("First", lines[0]);
    EXPECT_EQ("Second", lines[1]);
    EXPECT_EQ("Third", lines[2]);
}

TEST(OclocArgHelperTest, GivenValidSourceFileWhenReadingItViaHelperAsBinaryThenItIsReturned) {
    const char input[] = "First\nSecond\nThird";
    const auto inputLength{sizeof(input)};
    const auto filename{"some_file.txt"};

    Source source{reinterpret_cast<const uint8_t *>(input), inputLength, filename};

    MockOclocArgHelper::FilesMap mockArgHelperFilesMap{};
    MockOclocArgHelper mockOclocArgHelper{mockArgHelperFilesMap};

    mockOclocArgHelper.callBaseReadBinaryFile = true;
    mockOclocArgHelper.inputs.push_back(source);

    const auto fileContent = mockOclocArgHelper.readBinaryFile("some_file.txt");
    ASSERT_EQ(inputLength, fileContent.size());

    const auto isContentIdentical = (0 == std::memcmp(input, fileContent.data(), inputLength));
    EXPECT_TRUE(isContentIdentical);
}

} // namespace NEO
