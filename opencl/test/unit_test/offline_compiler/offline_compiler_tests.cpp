/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler_tests.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/offline_compiler/source/ocloc_interface.h"
#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debug_settings/debug_variables_helper.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_compiler_cache.h"
#include "shared/test/common/mocks/mock_compiler_product_helper.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_fcl_facade.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_igc_facade.h"

#include "device_ids_configs.h"
#include "environment.h"
#include "gtest/gtest.h"
#include "mock/mock_argument_helper.h"
#include "mock/mock_multi_command.h"
#include "mock/mock_offline_compiler.h"
#include "neo_aot_platforms.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

extern Environment *gEnvironment;

namespace NEO {
extern std::map<std::string, std::stringstream> virtualFileList;

void MultiCommandTests::createFileWithArgs(const std::vector<std::string> &singleArgs, int numOfBuild) {

    for (int i = 0; i < numOfBuild; i++) {
        for (auto singleArg : singleArgs) {
            filesMap[nameOfFileWithArgs.c_str()] += std::string(singleArg + " ");
        }
        filesMap[nameOfFileWithArgs.c_str()] += std::string("\n");
    }
}

void MultiCommandTests::deleteFileWithArgs() {
    filesMap.erase(nameOfFileWithArgs);
}

void MultiCommandTests::deleteOutFileList() {
    NEO::virtualFileList.erase(outFileList.c_str());
}

std::string getCompilerOutputFileName(const std::string &fileName, const std::string &type) {
    std::string fName(fileName);
    fName.append("_");
    fName.append(gEnvironment->devicePrefix);
    fName.append(".");
    fName.append(type);
    return fName;
}

bool compilerOutputExists(const std::string &fileName, const std::string &type) {
    return NEO::virtualFileList.find(getCompilerOutputFileName(fileName, type)) != NEO::virtualFileList.end();
}

template <typename SectionHeaders>
bool isAnyIrSectionDefined(const SectionHeaders &sectionHeaders) {
    const auto isIrSection = [](const auto &section) {
        return section.header && (section.header->type == Elf::SHT_OPENCL_SPIRV || section.header->type == Elf::SHT_OPENCL_LLVM_BINARY);
    };

    return std::any_of(sectionHeaders.begin(), sectionHeaders.end(), isIrSection);
}

TEST_F(MultiCommandTests, WhenBuildingMultiCommandThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::mkdirPtr)> mockCreateDir(&NEO::IoFunctions::mkdirPtr, [](const char *path) -> int { return 0; });

    nameOfFileWithArgs = "ImAMultiCommandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clCopybufferFilename.c_str(),
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
    VariableBackup<decltype(NEO::IoFunctions::mkdirPtr)> mockCreateDir(&NEO::IoFunctions::mkdirPtr, [](const char *path) -> int { return 0; });

    nameOfFileWithArgs = "ImAMultiCommandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-gen_file",
        "-file",
        clCopybufferFilename.c_str(),
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
    VariableBackup<decltype(NEO::IoFunctions::mkdirPtr)> mockCreateDir(&NEO::IoFunctions::mkdirPtr, [](const char *path) -> int { return 0; });

    nameOfFileWithArgs = "ImAMultiCommandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clCopybufferFilename.c_str(),
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
        EXPECT_FALSE(compilerOutputExists(outFileName, "gen"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "bin"));
    }

    deleteFileWithArgs();
    delete pMultiCommand;
}

TEST_F(MultiCommandTests, GivenSpecifiedOutputDirWithProductConfigValueWhenBuildingMultiCommandThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::mkdirPtr)> mockCreateDir(&NEO::IoFunctions::mkdirPtr, [](const char *path) -> int { return 0; });

    auto &allEnabledDeviceConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
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

    nameOfFileWithArgs = "ImAMultiCommandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        clCopybufferFilename.c_str(),
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
        EXPECT_FALSE(compilerOutputExists(outFileName, "gen"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "bin"));
    }

    deleteFileWithArgs();
    delete pMultiCommand;
}

TEST_F(MultiCommandTests, GivenMissingTextFileWithArgsWhenBuildingMultiCommandThenInvalidFileErrorIsReturned) {
    nameOfFileWithArgs = "ImANotExistedCommandFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "multi",
        "ImANaughtyFile.txt",
        "-q",
    };

    StreamCapture capture;
    capture.captureStdout();
    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get()));
    std::string output = capture.getCapturedStdout();

    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pMultiCommand);
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    debugManager.flags.PrintDebugMessages.set(false);
}
TEST_F(MultiCommandTests, GivenLackOfClFileWhenBuildingMultiCommandThenInvalidFileErrorIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::mkdirPtr)> mockCreateDir(&NEO::IoFunctions::mkdirPtr, [](const char *path) -> int { return 0; });

    nameOfFileWithArgs = "ImAMultiCommandMinimalGoodFile.txt";
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

    StreamCapture capture;
    capture.captureStdout();
    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal, oclocArgHelperWithoutInput.get()));
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(nullptr, pMultiCommand);
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    debugManager.flags.PrintDebugMessages.set(false);

    deleteFileWithArgs();
}
TEST_F(MultiCommandTests, GivenOutputFileListFlagWhenBuildingMultiCommandThenSuccessIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::mkdirPtr)> mockCreateDir(&NEO::IoFunctions::mkdirPtr, [](const char *path) -> int { return 0; });

    nameOfFileWithArgs = "ImAMultiCommandMinimalGoodFile.txt";
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
        clCopybufferFilename.c_str(),
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
        EXPECT_FALSE(compilerOutputExists(outFileName, "gen"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "bin"));
    }

    deleteFileWithArgs();
    deleteOutFileList();
    delete pMultiCommand;
}

TEST(MultiCommandWhiteboxTest, GivenVerboseModeWhenShowingResultsThenLogsArePrintedForEachBuild) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.retValues = {OCLOC_SUCCESS, OCLOC_INVALID_FILE};
    mockMultiCommand.quiet = false;

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockMultiCommand.showResults();
    const auto output = capture.getCapturedStdout();

    const auto maskedResult = result | OCLOC_INVALID_FILE;
    EXPECT_NE(OCLOC_SUCCESS, result);
    EXPECT_EQ(OCLOC_INVALID_FILE, maskedResult);

    const auto expectedOutput{"Build command 0: successful\n"
                              "Build command 1: failed. Error code: -5151\n"};
    EXPECT_EQ(expectedOutput, output);
}

TEST(MultiCommandWhiteboxTest, GivenVerboseModeAndDefinedOutputFilenameAndDirectoryWhenAddingAdditionalOptionsToSingleCommandLineThenNothingIsDone) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;

    std::vector<std::string> singleArgs = {
        "-file",
        "some_kernel.cl",
        "-output",
        "SpecialOutputFilename",
        "-out_dir",
        "SomeOutputDirectory",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    const auto singleArgsCopy{singleArgs};
    const size_t buildId{0};

    StreamCapture capture;
    capture.captureStdout();
    mockMultiCommand.addAdditionalOptionsToSingleCommandLine(singleArgs, buildId);
    const auto output = capture.getCapturedStdout();

    EXPECT_EQ(singleArgsCopy, singleArgs);
}

TEST(MultiCommandWhiteboxTest, GivenHelpArgumentsWhenInitializingThenHelpIsPrinted) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;

    std::vector<std::string> singleArgs = {
        "--help"};

    const auto args{singleArgs};

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockMultiCommand.initialize(args);
    const auto output = capture.getCapturedStdout();

    const auto expectedOutput = R"===(Compiles multiple files using a config file.

Usage: ocloc multi <file_name>
  <file_name>   Input file containing a list of arguments for subsequent
                ocloc invocations.
                Expected format of each line inside such file is:
                '-file <filename> -device <device_type> [compile_options]'.
                See 'ocloc compile --help' for available compile_options.
                Results of subsequent compilations will be dumped into 
                a directory with name identical file_name's base name.

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

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockMultiCommand.splitLineInSeparateArgs(outputArgs, commandLine, numberOfBuild);
    const auto output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_SUCCESS, result);
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

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockMultiCommand.splitLineInSeparateArgs(outputArgs, commandLine, numberOfBuild);
    const auto output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_INVALID_FILE, result);

    const auto expectedOutput = "One of the quotes is open in build number 1\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(MultiCommandWhiteboxTest, GivenCommandLineWithMissingApostropheWhenRunningBuildsThenErrorIsReturnedAndBuildIsNotStarted) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = true;
    mockMultiCommand.callBaseSingleBuild = false;
    mockMultiCommand.lines.push_back("-out_dir \"Some Directory");

    mockMultiCommand.argHelper->getPrinterRef().setSuppressMessages(true);
    mockMultiCommand.runBuilds("ocloc");
    EXPECT_EQ(0, mockMultiCommand.singleBuildCalledCount);

    ASSERT_EQ(1u, mockMultiCommand.retValues.size());
    EXPECT_EQ(OCLOC_INVALID_FILE, mockMultiCommand.retValues[0]);
}

TEST(MultiCommandWhiteboxTest, GivenTwoValidCommandLinesAndVerboseModeWhenRunningBuildsThenBuildsAreStartedReturnValuesAreStoredAndLogsArePrinted) {
    MockMultiCommand mockMultiCommand{};
    mockMultiCommand.quiet = false;
    mockMultiCommand.callBaseSingleBuild = false;

    const std::string validLine{"-file some_kernel.cl -output SpecialOutputFilename -out_dir SomeOutputDirectory -device " + gEnvironment->devicePrefix};
    mockMultiCommand.lines.push_back(validLine);
    mockMultiCommand.lines.push_back(validLine);

    StreamCapture capture;
    capture.captureStdout();
    mockMultiCommand.runBuilds("ocloc");
    const auto output = capture.getCapturedStdout();

    EXPECT_EQ(2, mockMultiCommand.singleBuildCalledCount);

    ASSERT_EQ(2u, mockMultiCommand.retValues.size());
    EXPECT_EQ(OCLOC_SUCCESS, mockMultiCommand.retValues[0]);
    EXPECT_EQ(OCLOC_SUCCESS, mockMultiCommand.retValues[1]);

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

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockMultiCommand.initialize(args);
    const auto output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_INVALID_FILE, result);

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

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockMultiCommand.initialize(args);
    const auto output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, result);

    const auto expectedError = "Invalid option (arg 3): -invalid_option\n";
    const auto errorPosition = output.find(expectedError);
    EXPECT_NE(std::string::npos, errorPosition);
}

using MockOfflineCompilerTests = ::testing::Test;
TEST_F(MockOfflineCompilerTests, givenProductConfigValueWhenInitHwInfoThenCorrectValueIsSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    auto config = AOT::UNKNOWN_ISA;
    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            config = static_cast<AOT::PRODUCT_CONFIG>(deviceMapConfig.aotConfig.value);
            break;
        }
    }

    mockOfflineCompiler.deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(config);
    EXPECT_FALSE(mockOfflineCompiler.deviceName.empty());

    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    EXPECT_EQ(mockOfflineCompiler.deviceConfig, config);
}

TEST_F(MockOfflineCompilerTests, givenDeviceIdsFromDevicesFileWhenInitHwInfoThenDeviceConfigValuesIsSet) {
    std::vector<unsigned short> deviceIds{
#define NAMEDDEVICE(devId, ignored_product, ignored_devName) devId,
#define DEVICE(devId, ignored_product) devId,
#include "devices.inl"
#undef DEVICE
#undef NAMEDDEVICE
    };
    if (deviceIds.empty()) {
        GTEST_SKIP();
    }
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.argHelper->getPrinterRef().setSuppressMessages(true);
    for (const auto &deviceId : deviceIds) {
        mockOfflineCompiler.deviceConfig = AOT::UNKNOWN_ISA;
        std::stringstream deviceIDStr;
        deviceIDStr << "0x" << std::hex << deviceId;
        EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initHardwareInfo(deviceIDStr.str())) << deviceIDStr.str();
        EXPECT_NE(mockOfflineCompiler.deviceConfig, AOT::UNKNOWN_ISA);
    }
}

TEST_F(MockOfflineCompilerTests, givenDeviceIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    uint32_t deviceID = 0;
    std::stringstream deviceIDStr, expectedOutput;
    std::string deviceStr;
    uint32_t productConfig = AOT::UNKNOWN_ISA;

    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            deviceID = deviceMapConfig.deviceIds->front();
            deviceIDStr << "0x" << std::hex << deviceID;
            deviceStr = mockOfflineCompiler.argHelper->productConfigHelper->getAcronymForProductConfig(deviceMapConfig.aotConfig.value);
            productConfig = deviceMapConfig.aotConfig.value;
            break;
        }
    }

    mockOfflineCompiler.deviceName = deviceIDStr.str();
    EXPECT_FALSE(mockOfflineCompiler.deviceName.empty());

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    std::string output = capture.getCapturedStdout();
    expectedOutput << "Auto-detected target based on " << deviceIDStr.str() << " device id: " << deviceStr << "\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
    EXPECT_EQ(mockOfflineCompiler.deviceConfig, productConfig);
}

TEST_F(MockOfflineCompilerTests, givenDeviceIdAndRevisionIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    uint32_t deviceID = 0;
    std::stringstream deviceIDStr, expectedOutput;
    std::string deviceStr;
    uint32_t productConfig = AOT::UNKNOWN_ISA;

    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            deviceID = deviceMapConfig.deviceIds->front();
            deviceIDStr << "0x" << std::hex << deviceID;
            deviceStr = mockOfflineCompiler.argHelper->productConfigHelper->getAcronymForProductConfig(deviceMapConfig.aotConfig.value);
            productConfig = deviceMapConfig.aotConfig.value;
            break;
        }
    }

    mockOfflineCompiler.deviceName = deviceIDStr.str();
    mockOfflineCompiler.revisionId = 0x0;
    EXPECT_FALSE(mockOfflineCompiler.deviceName.empty());

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName);
    std::string output = capture.getCapturedStdout();
    expectedOutput << "Auto-detected target based on " << deviceIDStr.str() << " device id: " << deviceStr << "\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, mockOfflineCompiler.revisionId);
    EXPECT_EQ(mockOfflineCompiler.deviceConfig, productConfig);
}

TEST_F(MockOfflineCompilerTests, givenProductConfigValueWhenInitHwInfoThenBaseHardwareInfoValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
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
        EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName));

        auto defaultIpVersion = mockOfflineCompiler.compilerProductHelper->getDefaultHwIpVersion();
        auto productConfig = mockOfflineCompiler.compilerProductHelper->matchRevisionIdWithProductConfig(defaultIpVersion, mockOfflineCompiler.revisionId);
        if (mockOfflineCompiler.argHelper->productConfigHelper->isSupportedProductConfig(productConfig)) {
            EXPECT_EQ(mockOfflineCompiler.hwInfo.ipVersion.value, productConfig);
        } else {
            EXPECT_EQ(mockOfflineCompiler.hwInfo.ipVersion.value, defaultIpVersion);
        }

        EXPECT_EQ(mockOfflineCompiler.revisionId, static_cast<decltype(mockOfflineCompiler.revisionId)>(mockOfflineCompiler.hwInfo.platform.usRevId));
    }
}

TEST_F(MockOfflineCompilerTests, givenDeprecatedAcronymThenSameAcronymIsReturnedAsProduct) {
    MockOfflineCompiler mockOfflineCompiler;
    auto deprecatedAcronyms = mockOfflineCompiler.argHelper->productConfigHelper->getDeprecatedAcronyms();
    if (deprecatedAcronyms.empty()) {
        GTEST_SKIP();
    }

    for (const auto &acronym : deprecatedAcronyms) {
        NEO::CompilerOptions::TokenizedString srcAcronym;
        srcAcronym.push_back(acronym);
        auto products = NEO::getProductForSpecificTarget(srcAcronym, mockOfflineCompiler.argHelper);
        ASSERT_EQ(1U, products.size());
        EXPECT_STREQ(acronym.data(), products[0].data());
    }
}

TEST_F(MockOfflineCompilerTests, givenValidAcronymAndRevisionIdWhenInitHardwareInfoForProductConfigThenInvalidDeviceIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            if (!deviceMapConfig.deviceAcronyms.empty()) {
                mockOfflineCompiler.deviceName = deviceMapConfig.deviceAcronyms.front().str();
                break;
            } else if (!deviceMapConfig.rtlIdAcronyms.empty()) {
                mockOfflineCompiler.deviceName = deviceMapConfig.rtlIdAcronyms.front().str();
                break;
            }
        }
    }
    if (mockOfflineCompiler.deviceName.empty()) {
        GTEST_SKIP();
    }

    mockOfflineCompiler.revisionId = 0x3;
    EXPECT_EQ(OCLOC_INVALID_DEVICE, mockOfflineCompiler.initHardwareInfoForProductConfig(mockOfflineCompiler.deviceName));
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
        EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OCLOC_SUCCESS);

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

        EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OCLOC_SUCCESS);

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
    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            if (!deviceMapConfig.deviceAcronyms.empty()) {
                mockOfflineCompiler.deviceName = deviceMapConfig.deviceAcronyms.front().str();
                break;
            } else if (!deviceMapConfig.rtlIdAcronyms.empty()) {
                mockOfflineCompiler.deviceName = deviceMapConfig.rtlIdAcronyms.front().str();
                break;
            }
        }
    }
    if (mockOfflineCompiler.deviceName.empty()) {
        GTEST_SKIP();
    }

    std::transform(mockOfflineCompiler.deviceName.begin(), mockOfflineCompiler.deviceName.end(), mockOfflineCompiler.deviceName.begin(), ::toupper);
    EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OCLOC_SUCCESS);
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
        EXPECT_EQ(mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName), OCLOC_SUCCESS);
    }
}

HWTEST2_F(MockOfflineCompilerTests, givenProductConfigValueWhenInitHwInfoThenMaxDualSubSlicesSupportedIsSet, MatchAny) {
    MockOfflineCompiler mockOfflineCompiler;
    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
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

TEST_F(MockOfflineCompilerTests, givenDeviceIpVersionWhenInitHwInfoThenCorrectResultsIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    std::stringstream config;
    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            config << deviceMapConfig.aotConfig.value;
            mockOfflineCompiler.deviceName = config.str();
            break;
        }
    }

    EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName));
}

TEST_F(MockOfflineCompilerTests, givenTheSameProductConfigWhenDifferentDeviceFormParameterIsPassedThenSameResultsAreReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    std::stringstream ipVersion;

    auto &allEnabledDeviceConfigs = mockOfflineCompiler.argHelper->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        GTEST_SKIP();
    }

    for (const auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            ipVersion << deviceMapConfig.aotConfig.value;
            mockOfflineCompiler.deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(deviceMapConfig.aotConfig);
            break;
        }
    }

    EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName));
    EXPECT_NE(AOT::UNKNOWN_ISA, mockOfflineCompiler.deviceConfig);
    auto expectedConfig = mockOfflineCompiler.deviceConfig;

    mockOfflineCompiler.deviceName = ipVersion.str();
    EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initHardwareInfo(mockOfflineCompiler.deviceName));
    EXPECT_EQ(expectedConfig, mockOfflineCompiler.deviceConfig);
}

TEST_F(OfflineCompilerTests, GivenHelpOptionOnQueryThenSuccessIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "query",
        "--help"};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = OfflineCompiler::query(argv.size(), argv, oclocArgHelperWithoutInput.get());
    std::string output = capture.getCapturedStdout();

    EXPECT_STREQ(OfflineCompiler::queryHelp.data(), output.c_str());
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
}

TEST_F(OfflineCompilerTests, GivenHelpOptionOnIdsThenSuccessIsReturned) {
    std::vector<ConstStringRef> helpFlags = {"-h", "--help"};
    for (const auto &helpFlag : helpFlags) {
        std::vector<std::string> argv = {
            "ocloc",
            "ids",
            helpFlag.str()};

        StreamCapture capture;
        capture.captureStdout();
        int retVal = OfflineCompiler::queryAcronymIds(argv.size(), argv, oclocArgHelperWithoutInput.get());
        std::string output = capture.getCapturedStdout();

        std::stringstream expectedOutput;
        expectedOutput << R"===(
Depending on <acronym> will return all
matched versions (<major>.<minor>.<revision>)
that correspond to the given name.
All supported acronyms: )===";
        expectedOutput << getSupportedDevices(oclocArgHelperWithoutInput.get()) << ".\n";

        EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
        EXPECT_EQ(OCLOC_SUCCESS, retVal);
    }
}

TEST_F(OfflineCompilerTests, givenFamilyAcronymWhenIdsCommandIsInvokeThenSuccessAndCorrectIdsAreReturned) {
    auto enabledFamilies = oclocArgHelperWithoutInput->productConfigHelper->getFamiliesAcronyms();
    if (enabledFamilies.empty()) {
        GTEST_SKIP();
    }
    auto &supportedDevicesConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    std::string familyAcronym = enabledFamilies.front().str();

    std::vector<std::string> expected{};
    auto family = oclocArgHelperWithoutInput->productConfigHelper->getFamilyFromDeviceName(familyAcronym);
    for (const auto &device : supportedDevicesConfigs) {
        if (device.family == family) {
            auto config = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
            expected.push_back(config);
        }
    }
    std::vector<std::string> argv = {
        "ocloc",
        "ids",
        familyAcronym};

    std::stringstream expectedOutput;

    StreamCapture capture;
    capture.captureStdout();
    int retVal = OfflineCompiler::queryAcronymIds(argv.size(), argv, oclocArgHelperWithoutInput.get());
    std::string output = capture.getCapturedStdout();
    expectedOutput << "Matched ids:\n";

    for (const auto &prefix : expected) {
        expectedOutput << prefix << "\n";
    }
    EXPECT_STREQ(expectedOutput.str().c_str(), output.c_str());
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
}

TEST_F(OfflineCompilerTests, givenReleaseAcronymWhenIdsCommandIsInvokeThenSuccessAndCorrectIdsAreReturned) {
    auto enabledReleases = oclocArgHelperWithoutInput->productConfigHelper->getReleasesAcronyms();
    if (enabledReleases.empty()) {
        GTEST_SKIP();
    }
    auto &supportedDevicesConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    const auto &releaseAcronym = enabledReleases.front();
    std::vector<std::string> expected{};
    auto release = oclocArgHelperWithoutInput->productConfigHelper->getReleaseFromDeviceName(releaseAcronym.str());
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

    StreamCapture capture;
    capture.captureStdout();
    int retVal = OfflineCompiler::queryAcronymIds(argv.size(), argv, oclocArgHelperWithoutInput.get());
    std::string output = capture.getCapturedStdout();
    expectedOutput << "Matched ids:\n";

    for (const auto &prefix : expected) {
        expectedOutput << prefix << "\n";
    }
    EXPECT_STREQ(expectedOutput.str().c_str(), output.c_str());
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
}

TEST_F(OfflineCompilerTests, givenProductAcronymWhenIdsCommandIsInvokeThenSuccessAndCorrectIdsAreReturned) {
    auto enabledProducts = oclocArgHelperWithoutInput->productConfigHelper->getAllProductAcronyms();
    if (enabledProducts.empty()) {
        GTEST_SKIP();
    }
    auto &supportedDevicesConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    const auto &productAcronym = enabledProducts.front();
    std::vector<std::string> expected{};
    auto product = ProductConfigHelper::getProductConfigFromAcronym(productAcronym.str());
    for (const auto &device : supportedDevicesConfigs) {
        if (device.aotConfig.value == product) {
            auto config = ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig);
            expected.push_back(config);
        }
    }
    std::vector<std::string> argv = {
        "ocloc",
        "ids",
        productAcronym.str()};

    std::stringstream expectedOutput;

    StreamCapture capture;
    capture.captureStdout();
    int retVal = OfflineCompiler::queryAcronymIds(argv.size(), argv, oclocArgHelperWithoutInput.get());
    std::string output = capture.getCapturedStdout();
    expectedOutput << "Matched ids:\n";

    for (const auto &prefix : expected) {
        expectedOutput << prefix << "\n";
    }
    EXPECT_STREQ(expectedOutput.str().c_str(), output.c_str());
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
}

TEST_F(OfflineCompilerTests, WhenQueryingSupportedDevicesThenNonEmptyOutputFileIsGenerated) {
    std::vector<Ocloc::SupportedDevicesMode> modes = {Ocloc::SupportedDevicesMode::concat, Ocloc::SupportedDevicesMode::merge};

    for (const auto &mode : modes) {
        std::vector<std::string> argv = {
            "ocloc",
            "query",
            "SUPPORTED_DEVICES",
            "-" + toStr(mode)};

        int retVal = OfflineCompiler::query(argv.size(), argv, oclocArgHelperWithoutInput.get());
        EXPECT_EQ(OCLOC_SUCCESS, retVal);

        Ocloc::SupportedDevicesHelper supportedDevicesHelper(mode, oclocArgHelperWithoutInput->productConfigHelper.get());
        std::string expectedFileName = supportedDevicesHelper.getCurrentOclocOutputFilename();

        EXPECT_NE(oclocArgHelperWithoutInput->filesMap.find(expectedFileName), oclocArgHelperWithoutInput->filesMap.end());

        std::string generatedContent = oclocArgHelperWithoutInput->filesMap[expectedFileName];
        EXPECT_FALSE(generatedContent.empty());

        EXPECT_EQ(OCLOC_SUCCESS, retVal);
    }
}

TEST_F(OfflineCompilerTests, GivenFlagsWhichRequireMoreArgsWithoutThemWhenParsingThenErrorIsReported) {
    const std::array<std::string, 9> flagsToTest = {
        "-file", "-output", "-device", "-options", "-internal_options", "-out_dir", "-cache_dir", "-revision_id", "-config"};

    for (const auto &flag : flagsToTest) {
        const std::vector<std::string> argv = {
            "ocloc",
            "compile",
            flag};

        MockOfflineCompiler mockOfflineCompiler{};

        StreamCapture capture;
        capture.captureStdout();
        const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
        const auto output{capture.getCapturedStdout()};

        EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, result);

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
            clCopybufferFilename.c_str(),
            flag,
            "-device",
            gEnvironment->devicePrefix.c_str()};

        MockOfflineCompiler mockOfflineCompiler{};

        const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
        EXPECT_EQ(OCLOC_SUCCESS, result);

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
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-config",
        configStr};

    MockOfflineCompiler mockOfflineCompiler{};
    uint64_t config = 0u;
    parseHwInfoConfigString(configStr, config);

    const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, result);
    EXPECT_EQ(mockOfflineCompiler.hwInfoConfig, config);
}

TEST_F(OfflineCompilerTests, givenIncorrectConfigFlagWhenParsingCommandLineThenErrorLogIsPrintedAndFailureIsReturned) {
    std::string configStr = "1xabcf";
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-config",
        configStr};

    MockOfflineCompiler mockOfflineCompiler{};

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, result);
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
            clCopybufferFilename.c_str(),
            flag,
            "-device",
            gEnvironment->devicePrefix.c_str()};

        MockOfflineCompiler mockOfflineCompiler{};

        const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
        EXPECT_EQ(OCLOC_SUCCESS, result);

        const auto is64BitModeSet{mockOfflineCompiler.internalOptions.find(CompilerOptions::arch64bit.data()) != std::string::npos};
        EXPECT_TRUE(is64BitModeSet);
    }
}

TEST_F(OfflineCompilerTests, Given32BitModeFlagAnd64BitModeFlagWhenParsingThenErrorLogIsPrintedAndFailureIsReturned) {
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        clCopybufferFilename.c_str(),
        "-32",
        "-64",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    const auto output{capture.getCapturedStdout()};

    EXPECT_NE(OCLOC_SUCCESS, result);

    const auto maskedResult = result | OCLOC_INVALID_COMMAND_LINE;
    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, maskedResult);

    const std::string expectedErrorMessage{"Error: Cannot compile for 32-bit and 64-bit, please choose one.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineCompilerTests, GivenFlagStringWhenParsingThenInternalBooleanIsSetAndSuccessIsReturned) {
    using namespace std::string_literals;

    const std::array flagsToTest = {
        std::pair{"-options_name"s, &MockOfflineCompiler::useOptionsSuffix},
        std::pair{"-gen_file"s, &MockOfflineCompiler::useGenFile}};

    for (const auto &[flagString, memberBoolean] : flagsToTest) {
        const std::vector<std::string> argv = {
            "ocloc",
            "compile",
            "-file",
            clCopybufferFilename.c_str(),
            flagString,
            "-device",
            gEnvironment->devicePrefix.c_str()};

        MockOfflineCompiler mockOfflineCompiler{};

        const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
        EXPECT_EQ(OCLOC_SUCCESS, result);

        EXPECT_TRUE(mockOfflineCompiler.*memberBoolean);
    }
}

TEST_F(OfflineCompilerTests, givenDeviceAsPvcHexIdAndDeviceOptionsStricteForPvcOnlyWhenCmdLineParsedThenHexIdIsTranslatedToAcronymAndDeviceOptionsAreApplied) {
    std::string deviceIdStr = "0x0bd5",
                relevantAcronymStr = "pvc",
                exampleDevOptionsStr = "-options -ze-opt-large-register-file";
    const std::vector<std::string> argv = {
        "ocloc",
        "-file", clFiles + "foo.spv",
        "-output_no_suffix",
        "-spirv_input",
        "-device_options", relevantAcronymStr, exampleDevOptionsStr,
        "-device", deviceIdStr,
        "-revision_id", "0x2f"};

    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto allEnabledDeviceAcronyms = mockOfflineCompiler->argHelper->productConfigHelper->getRepresentativeProductAcronyms();
    auto deprecatedAcronyms = mockOfflineCompiler->argHelper->productConfigHelper->getDeprecatedAcronyms();
    if ((allEnabledDeviceAcronyms.empty() || std::find(allEnabledDeviceAcronyms.begin(), allEnabledDeviceAcronyms.end(), relevantAcronymStr) == allEnabledDeviceAcronyms.end()) && (deprecatedAcronyms.empty() || std::find(deprecatedAcronyms.begin(), deprecatedAcronyms.end(), relevantAcronymStr) == deprecatedAcronyms.end())) {
        GTEST_SKIP();
    }

    const auto result = mockOfflineCompiler->parseCommandLine(argv.size(), argv);

    EXPECT_EQ(OCLOC_SUCCESS, result);
    EXPECT_STREQ(mockOfflineCompiler->options.c_str(), exampleDevOptionsStr.c_str());
}

TEST_F(OfflineCompilerTests, givenDeviceHexIdAndDeviceOptionsInGeneralWhenCmdLineParsedThenHexIdIsTranslatedToAcronymAndDeviceOptionsAreApplied) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });
    struct DeviceIdElementDescriptionType {
        unsigned short idVal;
        std::string idStr;
        std::string configStr;
        std::vector<std::string> acronymsResultants;
    };
    std::vector<DeviceIdElementDescriptionType> deviceIdsVec{
#define NAMEDDEVICE(devId, hwConf, ignored_brandingStr) {devId, #devId, #hwConf, {}},
#define DEVICE(devId, hwConf) {devId, #devId, #hwConf, {}},
#include "devices.inl"
#undef DEVICE
#undef NAMEDDEVICE
    };
    struct ConfigRelationElementDescriptionType {
        std::vector<unsigned short> devIdVals;
        std::string configStr;
        enum AOT::PRODUCT_CONFIG prodIpTypeVal;
    };
    std::vector<ConfigRelationElementDescriptionType> configRelationVec{
#define DEVICE_CONFIG(prodIpTypeVer, hwConf, devIds, ignored_family, ignored_release) {NEO::devIds, #hwConf, AOT::prodIpTypeVer},
#include "product_config.inl"
#undef DEVICE_CONFIG
    };

    if (deviceIdsVec.empty()) {
        GTEST_SKIP();
    }

    auto &deviceIdElement = deviceIdsVec.front();
    std::transform(deviceIdElement.idStr.begin(), deviceIdElement.idStr.end(), deviceIdElement.idStr.begin(), ::tolower);

    for (auto &configRelationElement : configRelationVec) {
        if (deviceIdElement.configStr != configRelationElement.configStr ||
            std::find(configRelationElement.devIdVals.begin(), configRelationElement.devIdVals.end(), deviceIdElement.idVal) == configRelationElement.devIdVals.end()) {
            continue;
        }

        auto addFirstMatchingAcronym = [&](const std::map<std::string, AOT::PRODUCT_CONFIG> &acronymMap) -> bool {
            for (const auto &acrMapEl : acronymMap) {
                if (acrMapEl.second == configRelationElement.prodIpTypeVal) {
                    deviceIdElement.acronymsResultants.push_back(acrMapEl.first);
                    return true;
                }
            }
            return false;
        };

        if (addFirstMatchingAcronym(AOT::deviceAcronyms) ||
            addFirstMatchingAcronym(AOT::getRtlIdAcronyms()) ||
            addFirstMatchingAcronym(AOT::genericIdAcronyms)) {
            break;
        }
    }

    if (deviceIdElement.acronymsResultants.empty()) {
        GTEST_SKIP();
    }

    std::string deviceIdStr = deviceIdElement.idStr;
    std::string relevantAcronymStr = deviceIdElement.acronymsResultants.front();
    std::string exampleDevOptionsStr = "any string with options e.g.: (-options)? -ze-opt-large-register-file";

    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file", clFiles + "foo.spv",
        "-output_no_suffix",
        "-spirv_input",
        "-device_options", relevantAcronymStr, exampleDevOptionsStr,
        "-device", deviceIdStr};

    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    ASSERT_NE(nullptr, mockOfflineCompiler);

    const auto result = mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, result);

    EXPECT_STREQ(mockOfflineCompiler->options.c_str(), exampleDevOptionsStr.c_str());
}

TEST_F(OfflineCompilerTests, WhenPickingFormatFromCommandLineThenCodeTypeIsSetAccordingly) {
    std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    {
        argv.push_back("-llvm_bc");
        MockOfflineCompiler mockOfflineCompiler{};
        EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.parseCommandLine(argv.size(), argv));
        EXPECT_EQ(IGC::CodeType::oclC, mockOfflineCompiler.inputCodeType);
        EXPECT_EQ(IGC::CodeType::llvmBc, mockOfflineCompiler.intermediateRepresentation);
        argv.pop_back();
    }

    {
        argv.push_back("-llvm_text");
        MockOfflineCompiler mockOfflineCompiler{};
        EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.parseCommandLine(argv.size(), argv));
        EXPECT_EQ(IGC::CodeType::oclC, mockOfflineCompiler.inputCodeType);
        EXPECT_EQ(IGC::CodeType::llvmLl, mockOfflineCompiler.intermediateRepresentation);
        argv.pop_back();
    }

    {
        argv.push_back("-spirv_input");
        MockOfflineCompiler mockOfflineCompiler{};
        EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.parseCommandLine(argv.size(), argv));
        EXPECT_EQ(IGC::CodeType::spirV, mockOfflineCompiler.inputCodeType);
        EXPECT_EQ(IGC::CodeType::spirV, mockOfflineCompiler.intermediateRepresentation);
        argv.pop_back();
    }

    argv.push_back("-llvm_input");
    {
        argv.push_back("-llvm_bc");
        MockOfflineCompiler mockOfflineCompiler{};
        EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.parseCommandLine(argv.size(), argv));
        EXPECT_EQ(IGC::CodeType::llvmBc, mockOfflineCompiler.inputCodeType);
        EXPECT_EQ(IGC::CodeType::llvmBc, mockOfflineCompiler.intermediateRepresentation);
        argv.pop_back();
    }

    {
        argv.push_back("-llvm_text");
        MockOfflineCompiler mockOfflineCompiler{};
        EXPECT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.parseCommandLine(argv.size(), argv));
        EXPECT_EQ(IGC::CodeType::llvmLl, mockOfflineCompiler.inputCodeType);
        EXPECT_EQ(IGC::CodeType::llvmLl, mockOfflineCompiler.intermediateRepresentation);
        argv.pop_back();
    }
}

TEST_F(OfflineCompilerTests, GivenArgsWhenQueryIsCalledThenSuccessIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "query",
        "NEO_REVISION"};

    oclocArgHelperWithoutInput->messagePrinter.setSuppressMessages(true);
    int retVal = OfflineCompiler::query(argv.size(), argv, oclocArgHelperWithoutInput.get());

    EXPECT_EQ(OCLOC_SUCCESS, retVal);
}

TEST_F(OfflineCompilerTests, GivenArgsWhenOfflineCompilerIsCreatedThenSuccessIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, givenDeviceIdHexValueWhenInitHwInfoThenItHasCorrectlySetValues) {
    auto deviceAotInfo = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (deviceAotInfo.empty()) {
        GTEST_SKIP();
    }

    MockOfflineCompiler mockOfflineCompiler;
    std::stringstream deviceString, productString;
    auto deviceId = deviceAotInfo[0].deviceIds->front();
    deviceString << "0x" << std::hex << deviceId;

    mockOfflineCompiler.argHelper->getPrinterRef().setSuppressMessages(true);
    mockOfflineCompiler.initHardwareInfo(deviceString.str());
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceId);
}

TEST_F(OfflineCompilerTests, givenProperDeviceIdHexAsDeviceArgumentThenSuccessIsReturned) {
    auto deviceAotInfo = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (deviceAotInfo.empty()) {
        GTEST_SKIP();
    }

    std::stringstream deviceString, productString;
    AOT::PRODUCT_CONFIG config = static_cast<AOT::PRODUCT_CONFIG>(deviceAotInfo[0].aotConfig.value);
    auto deviceId = deviceAotInfo[0].deviceIds->front();

    productString << oclocArgHelperWithoutInput->productConfigHelper->getAcronymForProductConfig(config);
    deviceString << "0x" << std::hex << deviceId;

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        deviceString.str()};

    oclocArgHelperWithoutInput->getPrinterRef().setSuppressMessages(false);

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    EXPECT_EQ(pOfflineCompiler->getHardwareInfo().platform.usDeviceID, deviceId);

    auto output = capture.getCapturedStdout();
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
        clCopybufferFilename.c_str(),
        "-device",
        "0x0"};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 0x0.\nError: Cannot get HW Info for device 0x0.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWithMissingRevisionValueWhenInvalidPatternIsPassedThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        "9.1."};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 9.1..\nError: Cannot get HW Info for device 9.1..\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWithInvalidPatternThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        "9.1.."};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 9.1...\nError: Cannot get HW Info for device 9.1...\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWithMissingMajorValueWhenInvalidPatternIsPassedThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        ".1.2"};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: .1.2.\nError: Cannot get HW Info for device .1.2.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWhenInvalidRevisionValueIsPassedThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        "9.0.a"};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 9.0.a.\nError: Cannot get HW Info for device 9.0.a.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWhenInvalidMinorValueIsPassedThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        "9.a"};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 9.a.\nError: Cannot get HW Info for device 9.a.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceNumerationWhenPassedValuesAreOutOfRangeThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        "256.350"};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 256.350.\nError: Cannot get HW Info for device 256.350.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDeviceIpVersionWhenPassedValueNotExistThenInvalidDeviceIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        "1234"};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 1234.\nError: Cannot get HW Info for device 1234.\n");
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
        clCopybufferFilename.c_str(),
        "-device",
        "0xnonexist"};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    auto output = capture.getCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STREQ(output.c_str(), "Could not determine device target: 0xnonexist.\nError: Cannot get HW Info for device 0xnonexist.\n");
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, givenDebugOptionThenInternalOptionShouldNotContainKernelDebugEnable) {
    if (gEnvironment->devicePrefix == "bdw") {
        GTEST_SKIP();
    }

    std::vector<std::string> argv = {
        "ocloc",
        "-options",
        "-g",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->initialize(argv.size(), argv);

    std::string internalOptions = mockOfflineCompiler->internalOptions;
    EXPECT_FALSE(hasSubstr(internalOptions, "-cl-kernel-debug-enable"));
}

TEST_F(OfflineCompilerTests, givenDebugOptionAndNonSpirvInputThenOptionsShouldContainDashSOptionAppendedAutomatically) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    mockOfflineCompiler->uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler->uniqueHelper->callBaseLoadDataFromFile = false;
    mockOfflineCompiler->uniqueHelper->filesMap["some_input.cl"] = "";

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-options",
        "-g",
        "-file",
        "some_input.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    mockOfflineCompiler->initialize(argv.size(), argv);
    const auto &options = mockOfflineCompiler->options;
    std::string appendedOption{"-s \"some_input.cl\""};
    EXPECT_TRUE(hasSubstr(options, appendedOption));
}

TEST_F(OfflineCompilerTests, givenDebugOptionAndNonSpirvInputWhenFilenameIsSeparatedWithSpacesThenAppendedSourcePathIsSetCorrectly) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    mockOfflineCompiler->uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler->uniqueHelper->callBaseLoadDataFromFile = false;
    mockOfflineCompiler->uniqueHelper->filesMap["filename with spaces.cl"] = "";

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-options",
        "-g",
        "-file",
        "filename with spaces.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    const auto result = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, result);
    const auto &options = mockOfflineCompiler->options;
    std::string appendedOption{"-s \"filename with spaces.cl\""};
    EXPECT_TRUE(hasSubstr(options, appendedOption));
}

TEST_F(OfflineCompilerTests, givenDebugOptionAndSpirvInputThenDoNotAppendDashSOptionAutomatically) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    mockOfflineCompiler->uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler->uniqueHelper->callBaseLoadDataFromFile = false;
    mockOfflineCompiler->uniqueHelper->filesMap["some_input.spirv"] = "";

    std::vector<std::string> argvSpirvInput = {
        "ocloc",
        "-q",
        "-spirv_input",
        "-options",
        "-g",
        "-file",
        "some_input.spirv",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    mockOfflineCompiler->initialize(argvSpirvInput.size(), argvSpirvInput);
    const auto &options = mockOfflineCompiler->options;
    std::string notAppendedOption{"-s \"some_input.spirv\""};
    EXPECT_FALSE(hasSubstr(options, notAppendedOption));
}

TEST_F(OfflineCompilerTests, givenDebugOptionWhenCompilerIsCMCThenDoNotAppendDashSOptionAutomatically) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    mockOfflineCompiler->uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler->uniqueHelper->callBaseLoadDataFromFile = false;
    mockOfflineCompiler->uniqueHelper->filesMap["some_input.cl"] = "";

    std::vector<std::string> argvSpirvInput = {
        "ocloc",
        "-q",
        "-options",
        "-g -cmc",
        "-file",
        "some_input.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    mockOfflineCompiler->initialize(argvSpirvInput.size(), argvSpirvInput);
    const auto &options = mockOfflineCompiler->options;
    std::string notAppendedOption{"-s \"some_input.spirv\""};
    EXPECT_FALSE(hasSubstr(options, notAppendedOption));
}

TEST_F(OfflineCompilerTests, givenDebugOptionAndDashSOptionPassedManuallyThenDoNotAppendDashSOptionAutomatically) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    mockOfflineCompiler->uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler->uniqueHelper->callBaseLoadDataFromFile = false;
    mockOfflineCompiler->uniqueHelper->filesMap["some_input.cl"] = "";
    std::vector<std::string> argvDashSPassed = {
        "ocloc",
        "-q",
        "-options",
        "-g -s \"mockPath/some_input.cl\"",
        "-file",
        "some_input.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    mockOfflineCompiler->initialize(argvDashSPassed.size(), argvDashSPassed);
    const auto &options = mockOfflineCompiler->options;
    std::string appendedOption{"-s"};
    auto occurrences = 0u;
    size_t pos = 0u;
    while ((pos = options.find(appendedOption, pos)) != std::string::npos) {
        occurrences++;
        pos++;
    }
    EXPECT_EQ(1u, occurrences);
}

TEST_F(OfflineCompilerTests, givenDashGInBiggerOptionStringWhenInitializingThenInternalOptionsShouldNotContainKernelDebugEnable) {

    std::vector<std::string> argv = {
        "ocloc",
        "-options",
        "-gNotRealDashGOption",
        "-file",
        clCopybufferFilename.c_str(),
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
        clCopybufferFilename.c_str(),
        "-internal_options",
        "-ze-exclude-ir-from-zebin",
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
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.mockFclFacade->shouldFailLoadingOfFclLib = true;
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;

    StreamCapture capture;
    capture.captureStdout();
    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, initResult);
    auto output = capture.getCapturedStdout();

    capture.captureStdout();
    const auto buildResult = mockOfflineCompiler.build();
    output = capture.getCapturedStdout();

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error! Loading of FCL library has failed! Filename: " << Os::frontEndDllName << "\n"
                         << "Error! FCL initialization failure. Error code = -6\n";

    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, buildResult);
    EXPECT_EQ(expectedErrorMessage.str(), output);
}

TEST_F(OfflineCompilerTests, givenValidArgumentsAndIgcInitFailureWhenInitIsPerformedThenFailureIsPropagatedAndErrorIsPrinted) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;
    std::string libName = "invalidigc.so";
    auto igcNameGuard = NEO::pushIgcDllName(libName.c_str());
    mockOfflineCompiler.mockIgcFacade->shouldFailLoadingOfIgcLib = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, initResult);
    auto output = capture.getCapturedStdout();

    capture.captureStdout();
    const auto buildResult = mockOfflineCompiler.build();
    output = capture.getCapturedStdout();

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error! Loading of IGC library has failed! Filename: " << libName << "\n"
                         << "Error! IGC initialization failure. Error code = -6\n";

    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, buildResult);
    EXPECT_EQ(expectedErrorMessage.str(), output);
}

TEST_F(OfflineCompilerTests, givenExcludeIrArgumentWhenInitIsPerformedThenIrExcludeFlagsShouldBeUnified) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-exclude_ir",
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
        clCopybufferFilename.c_str(),
        "-exclude_ir",
        "-internal_options",
        "-ze-exclude-ir-from-zebin",
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

bool containsAnyIRSection(ArrayRef<const uint8_t> elfBinary, std::string &outErrors, std::string &outWarnings) {
    if (Elf::isElf<Elf::EI_CLASS_32>(elfBinary)) {
        auto elf = Elf::decodeElf<Elf::EI_CLASS_32>(elfBinary, outErrors, outWarnings);
        return isAnyIrSectionDefined(elf.sectionHeaders);
    }
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(elfBinary, outErrors, outWarnings);
    return isAnyIrSectionDefined(elf.sectionHeaders);
}

TEST_F(OfflineCompilerTests, givenZeroSizeInputFileWhenInitializationIsPerformedThenInvalidFileIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->callBaseLoadDataFromFile = false;
    mockOfflineCompiler.uniqueHelper->shouldLoadDataFromFileReturnZeroSize = true;
    mockOfflineCompiler.uniqueHelper->messagePrinter.setSuppressMessages(true);

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, initResult);
    const auto buildResult = mockOfflineCompiler.build();
    EXPECT_EQ(OCLOC_INVALID_FILE, buildResult);
}

TEST_F(OfflineCompilerTests, givenInvalidIgcOutputWhenCompilingKernelThenOutOfHostMemoryIsReturned) {
    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.shouldReturnInvalidTranslationOutput = true;
    NEO::setIgcDebugVars(igcDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, buildResult);

    NEO::setIgcDebugVars(gEnvironment->igcDebugVars);
}

TEST_F(OfflineCompilerTests, givenIgcBuildFailureWhenCompilingKernelThenBuildProgramFailureIsReturned) {
    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.forceBuildFailure = true;
    NEO::setIgcDebugVars(igcDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OCLOC_BUILD_PROGRAM_FAILURE, buildResult);

    NEO::setIgcDebugVars(gEnvironment->igcDebugVars);
}

TEST_F(OfflineCompilerTests, givenInvalidFclOutputWhenCompilingKernelThenOutOfHostMemoryIsReturned) {
    MockCompilerDebugVars fclDebugVars{gEnvironment->fclDebugVars};
    fclDebugVars.shouldReturnInvalidTranslationOutput = true;
    NEO::setFclDebugVars(fclDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, buildResult);

    NEO::setFclDebugVars(gEnvironment->fclDebugVars);
}

TEST_F(OfflineCompilerTests, givenFclTranslationContextCreationFailureWhenCompilingKernelThenOutOfHostMemoryIsReturned) {
    MockCompilerDebugVars fclDebugVars{gEnvironment->fclDebugVars};
    fclDebugVars.shouldFailCreationOfTranslationContext = true;
    NEO::setFclDebugVars(fclDebugVars);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, buildResult);

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
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, initResult);

    const auto buildResult{mockOfflineCompiler.build()};
    EXPECT_EQ(OCLOC_BUILD_PROGRAM_FAILURE, buildResult);

    EXPECT_EQ("Error: Could not create context!", mockOfflineCompiler.getBuildLog());

    NEO::setFclDebugVars(gEnvironment->fclDebugVars);
}

TEST_F(OfflineCompilerTests, givenVariousClStdValuesWhenCompilingSourceThenCorrectExtensionsArePassed) {
    std::string clStdOptionValues[] = {"", "-cl-std=CL1.2", "-cl-std=CL2.0", "-cl-std=CL3.0"};

    for (auto &clStdOptionValue : clStdOptionValues) {
        std::vector<std::string> argv = {
            "ocloc",
            "-file",
            clCopybufferFilename.c_str(),
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
        auto compilerProductHelper = CompilerProductHelper::create(mockOfflineCompiler->hwInfo.platform.eProductFamily);
        getOpenclCFeaturesList(mockOfflineCompiler->hwInfo, openclCFeatures, *compilerProductHelper.get(), nullptr);
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
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    StreamCapture capture;
    capture.captureStdout();
    retVal = pOfflineCompiler->build();
    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bc") || compilerOutputExists("some_kernel", "spv"));
    EXPECT_FALSE(compilerOutputExists("some_kernel", "gen"));
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bin"));

    std::string buildLog = pOfflineCompiler->getBuildLog();
    EXPECT_STREQ(buildLog.c_str(), "");

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenArgsWhenBuildingWithDeviceConfigValueThenBuildSucceeds) {
    auto &allEnabledDeviceConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
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
        clCopybufferFilename.c_str(),
        "-device",
        configStr};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    StreamCapture capture;
    capture.captureStdout();
    retVal = pOfflineCompiler->build();
    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bc") || compilerOutputExists("some_kernel", "spv"));
    EXPECT_FALSE(compilerOutputExists("some_kernel", "gen"));
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bin"));

    std::string buildLog = pOfflineCompiler->getBuildLog();
    EXPECT_STREQ(buildLog.c_str(), "");

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenArgsWhenBuildingWithDeviceIpVersionValueThenBuildSucceeds) {
    auto &allEnabledDeviceConfigs = oclocArgHelperWithoutInput->productConfigHelper->getDeviceAotInfo();
    if (allEnabledDeviceConfigs.empty()) {
        return;
    }

    std::stringstream ipVersion;
    for (auto &deviceMapConfig : allEnabledDeviceConfigs) {
        if (productFamily == deviceMapConfig.hwInfo->platform.eProductFamily) {
            ipVersion << deviceMapConfig.aotConfig.value;
            break;
        }
    }

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        ipVersion.str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    StreamCapture capture;
    capture.captureStdout();
    retVal = pOfflineCompiler->build();
    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bc") || compilerOutputExists("some_kernel", "spv"));
    EXPECT_FALSE(compilerOutputExists("some_kernel", "gen"));
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bin"));

    std::string buildLog = pOfflineCompiler->getBuildLog();
    EXPECT_STREQ(buildLog.c_str(), "");

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenLlvmTextWhenBuildingThenBuildSucceeds) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-llvm_text"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("some_kernel", "ll"));
    EXPECT_FALSE(compilerOutputExists("some_kernel", "gen"));
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bin"));

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, WhenGenFileFlagIsNotProvidedThenGenFileIsNotCreated) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "copybuffer.cl"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr);
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr);
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr);

    NEO::IoFunctions::fseekPtr = [](FILE *stream, long int offset, int origin) -> int {
        return 0;
    };
    NEO::IoFunctions::ftellPtr = [](FILE *stream) -> long int {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        fileStream.seekg(0, std::ios::end);
        return static_cast<long int>(fileStream.tellg());
    };
    NEO::IoFunctions::freadPtr = [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        size_t totalBytes = size * count;
        fileStream.read(static_cast<char *>(ptr), totalBytes);
        return static_cast<size_t>(fileStream.gcount() / size);
    };

    uint32_t numOutputs = 0u;
    uint64_t *lenOutputs = nullptr;
    uint8_t **dataOutputs = nullptr;
    char **nameOutputs = nullptr;

    bool isSpvFile = false;
    bool isGenFile = false;
    bool isBinFile = false;
    std::string filePath = clFiles + "copybuffer.cl";

    const char *argv[] = {
        "ocloc",
        "-q",
        "-file",
        filePath.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    unsigned int argc = sizeof(argv) / sizeof(const char *);
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(numOutputs, 4u);

    for (unsigned int i = 0; i < numOutputs; i++) {
        std::string nameOutput(nameOutputs[i]);
        if (nameOutput.find(".spv") != std::string::npos) {
            isSpvFile = true;
        }
        if (nameOutput.find(".gen") != std::string::npos) {
            isGenFile = true;
        }
        if (nameOutput.find(".bin") != std::string::npos) {
            isBinFile = true;
        }
    }

    EXPECT_TRUE(isSpvFile);
    EXPECT_FALSE(isGenFile);
    EXPECT_TRUE(isBinFile);

    oclocFreeOutput(&numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
}

TEST_F(OfflineCompilerTests, WhenGenFileFlagIsProvidedThenGenFileIsCreated) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "copybuffer.cl"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr);
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr);
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr);

    NEO::IoFunctions::fseekPtr = [](FILE *stream, long int offset, int origin) -> int {
        return 0;
    };
    NEO::IoFunctions::ftellPtr = [](FILE *stream) -> long int {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        fileStream.seekg(0, std::ios::end);
        return static_cast<long int>(fileStream.tellg());
    };
    NEO::IoFunctions::freadPtr = [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
        std::string kernelSource = "example_kernel(){}";
        std::stringstream fileStream(kernelSource);
        size_t totalBytes = size * count;
        fileStream.read(static_cast<char *>(ptr), totalBytes);
        return static_cast<size_t>(fileStream.gcount() / size);
    };

    uint32_t numOutputs = 0u;
    uint64_t *lenOutputs = nullptr;
    uint8_t **dataOutputs = nullptr;
    char **nameOutputs = nullptr;
    std::string filePath = clFiles + "copybuffer.cl";

    bool isSpvFile = false;
    bool isGenFile = false;
    bool isBinFile = false;

    const char *argv[] = {
        "ocloc",
        "-q",
        "-gen_file",
        "-file",
        filePath.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    unsigned int argc = sizeof(argv) / sizeof(const char *);
    int retVal = oclocInvoke(argc, argv,
                             0, nullptr, nullptr, nullptr,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);

    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(numOutputs, 5u);

    for (unsigned int i = 0; i < numOutputs; i++) {
        std::string nameOutput(nameOutputs[i]);
        if (nameOutput.find(".spv") != std::string::npos) {
            isSpvFile = true;
        }
        if (nameOutput.find(".gen") != std::string::npos) {
            isGenFile = true;
        }
        if (nameOutput.find(".bin") != std::string::npos) {
            isBinFile = true;
        }
    }

    EXPECT_TRUE(isSpvFile);
    EXPECT_TRUE(isGenFile);
    EXPECT_TRUE(isBinFile);

    oclocFreeOutput(&numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
}

TEST_F(OfflineCompilerTests, WhenFclNotNeededThenDontLoadIt) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spirv_input"};

    MockOfflineCompiler offlineCompiler;
    offlineCompiler.uniqueHelper->filesMap = filesMap;

    StreamCapture capture;
    capture.captureStdout();
    auto ret = offlineCompiler.initialize(argv.size(), argv, true);
    EXPECT_EQ(OCLOC_SUCCESS, ret);
    ret = offlineCompiler.build();
    EXPECT_EQ(OCLOC_SUCCESS, ret);
    EXPECT_FALSE(offlineCompiler.fclFacade->isInitialized());
    EXPECT_TRUE(offlineCompiler.igcFacade->isInitialized());

    std::string output = capture.getCapturedStdout();
    auto expectedString = "Compilation from IR - skipping loading of FCL";

    EXPECT_TRUE(hasSubstr(output, expectedString));
}

TEST_F(OfflineCompilerTests, WhenParsingBinToCharArrayThenCorrectFileIsGenerated) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
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

    std::string devicePrefix = gEnvironment->devicePrefix;
    std::string fileName = "scheduler";
    std::string retArray = pOfflineCompiler->parseBinAsCharArray(binary, sizeof(binary), fileName);
    std::string target = "#include <cstddef>\n"
                         "#include <cstdint>\n\n"
                         "size_t SchedulerBinarySize_" +
                         devicePrefix + " = 37;\n"
                                        "uint32_t SchedulerBinary_" +
                         devicePrefix + "[10] = {\n"
                                        "    0x40032302, 0x90800756, 0x05340301, 0x66097860, 0x101010ff, 0x40032302, 0x90800756, 0x05340301, \n"
                                        "    0x66097860, 0xff000000};\n";
    EXPECT_EQ(retArray, target);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenCppFileWhenBuildingThenBuildSucceeds) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-cpp_file"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("some_kernel", "cpp"));
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bc") || compilerOutputExists("some_kernel", "spv"));
    EXPECT_FALSE(compilerOutputExists("some_kernel", "gen"));
    EXPECT_TRUE(compilerOutputExists("some_kernel", "bin"));

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, GivenOutputDirWhenBuildingThenBuildSucceeds) {
    VariableBackup<decltype(NEO::IoFunctions::mkdirPtr)> mockCreateDir(&NEO::IoFunctions::mkdirPtr, [](const char *path) -> int { return 0; });

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-out_dir",
        "offline_compiler_test"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/some_kernel", "bc") || compilerOutputExists("offline_compiler_test/some_kernel", "spv"));
    EXPECT_FALSE(compilerOutputExists("offline_compiler_test/some_kernel", "gen"));
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/some_kernel", "bin"));

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, GivenHelpOptionThenBuildDoesNotOccur) {
    std::vector<std::string> argv = {
        "ocloc",
        "--help"};
    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = capture.getCapturedStdout();
    EXPECT_STRNE("", output.c_str());
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, GivenInvalidFileWhenBuildingThenInvalidFileErrorIsReturned) {
    debugManager.flags.PrintDebugMessages.set(true);
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    retVal = pOfflineCompiler->build();
    std::string output = capture.getCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    debugManager.flags.PrintDebugMessages.set(false);
    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenInvalidFlagWhenBuildingThenInvalidCommandLineErrorIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-n",
        clFiles + "ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = capture.getCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenInvalidOptionsWhenBuildingThenInvalidCommandLineErrorIsReturned) {
    std::vector<std::string> argvA = {
        "ocloc",
        "-file",
    };

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argvA.size(), argvA, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = capture.getCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");

    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;

    std::vector<std::string> argvB = {
        "ocloc",
        "-file",
        clFiles + "ImANaughtyFile.cl",
        "-device"};

    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argvB.size(), argvB, true, retVal, oclocArgHelperWithoutInput.get());
    output = capture.getCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenNonexistantDeviceWhenCompilingThenInvalidDeviceErrorAndErrorMessageAreReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        "foobar"};

    StreamCapture capture;
    capture.captureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());
    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ(output.c_str(), "Could not determine device target: foobar.\nError: Cannot get HW Info for device foobar.\n");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, GivenInvalidKernelWhenBuildingThenBuildProgramFailureErrorIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-qq",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get());

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    gEnvironment->SetInputFileName("invalid_file_name");

    StreamCapture capture;
    capture.captureStdout();

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ(output.c_str(), "");

    std::string buildLog = pOfflineCompiler->getBuildLog();
    EXPECT_STRNE(buildLog.c_str(), "");

    gEnvironment->SetInputFileName("copybuffer");

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenAllowCachingWhenBuildingThenBinaryIsCached) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-allow_caching",
        "-cache_dir",
        "cache_dir"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->interceptCreatedDirs = true;
    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    auto retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto cacheMock = new CompilerCacheMock();
    mockOfflineCompiler->cache.reset(cacheMock);
    retVal = mockOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(cacheMock->cacheInvoked, 0u);
}

TEST_F(OfflineCompilerTests, GivenCachedBinaryWhenBuildToIrBinaryThenIrBinaryIsLoaded) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-allow_caching"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    mockOfflineCompiler->interceptCreatedDirs = true;
    auto retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto cacheMock = new CompilerCacheMock();
    cacheMock->loadResult = true;
    mockOfflineCompiler->cache.reset(cacheMock);
    retVal = mockOfflineCompiler->buildToIrBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, mockOfflineCompiler->irBinary);
    EXPECT_NE(0u, static_cast<uint32_t>(mockOfflineCompiler->irBinarySize));
}

TEST_F(OfflineCompilerTests, GivenCachedBinaryWhenBuildSourceCodeThenSuccessIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-allow_caching"};

    {
        auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
        ASSERT_NE(nullptr, mockOfflineCompiler);
        mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
        mockOfflineCompiler->interceptCreatedDirs = true;
        auto retVal = mockOfflineCompiler->initialize(argv.size(), argv);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto cacheMock = new CompilerCacheMock();
        mockOfflineCompiler->cache.reset(cacheMock);
        cacheMock->numberOfLoadResult = 1u;
        mockOfflineCompiler->sourceCode = "__kernel void k(){}";
        retVal = mockOfflineCompiler->buildSourceCode();
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, mockOfflineCompiler->genBinary);
        EXPECT_NE(0u, static_cast<uint32_t>(mockOfflineCompiler->genBinarySize));
    }

    {
        auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
        ASSERT_NE(nullptr, mockOfflineCompiler);
        mockOfflineCompiler->interceptCreatedDirs = true;
        auto retVal = mockOfflineCompiler->initialize(argv.size(), argv);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto cacheMock = new CompilerCacheMock();
        mockOfflineCompiler->cache.reset(cacheMock);
        cacheMock->numberOfLoadResult = 2u;
        mockOfflineCompiler->sourceCode = "__kernel void k(){}";
        retVal = mockOfflineCompiler->buildSourceCode();
        EXPECT_EQ(CL_SUCCESS, retVal);

        EXPECT_NE(nullptr, mockOfflineCompiler->genBinary);
        EXPECT_NE(0u, static_cast<uint32_t>(mockOfflineCompiler->genBinarySize));
    }

    {
        auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
        argv.push_back("-options");
        argv.push_back("-g");
        mockOfflineCompiler->interceptCreatedDirs = true;
        auto retVal = mockOfflineCompiler->initialize(argv.size(), argv);
        EXPECT_EQ(CL_SUCCESS, retVal);

        auto cacheMock = new CompilerCacheMock();
        mockOfflineCompiler->cache.reset(cacheMock);
        cacheMock->numberOfLoadResult = 3u;
        mockOfflineCompiler->sourceCode = "__kernel void k(){}";
        retVal = mockOfflineCompiler->buildSourceCode();
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, mockOfflineCompiler->debugDataBinary);
        EXPECT_NE(0u, static_cast<uint32_t>(mockOfflineCompiler->debugDataBinarySize));
    }
}

TEST_F(OfflineCompilerTests, GivenGenBinaryWhenGenerateElfBinaryThenElfIsLoaded) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-allow_caching"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    mockOfflineCompiler->interceptCreatedDirs = true;
    auto retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto cacheMock = new CompilerCacheMock();
    cacheMock->loadResult = true;
    mockOfflineCompiler->cache.reset(cacheMock);
    mockOfflineCompiler->genBinary = new char[1];
    mockOfflineCompiler->genBinarySize = sizeof(char);
    retVal = mockOfflineCompiler->generateElfBinary();
    EXPECT_TRUE(retVal);
    EXPECT_FALSE(mockOfflineCompiler->elfBinary.empty());
    EXPECT_NE(0u, static_cast<uint32_t>(mockOfflineCompiler->elfBinarySize));
}

TEST_F(OfflineCompilerTests, givenAllowCachingWhenBuildSourceCodeThenGenBinaryIsCachedUsingHashBasedOnNonNullIrBinary) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-allow_caching"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    mockOfflineCompiler->interceptCreatedDirs = true;
    auto retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto cacheMock = new CompilerCacheMock();
    mockOfflineCompiler->cache.reset(cacheMock);
    mockOfflineCompiler->sourceCode = "__kernel void k(){}";
    retVal = mockOfflineCompiler->buildSourceCode();

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, mockOfflineCompiler->irBinary);
    ASSERT_NE(0u, mockOfflineCompiler->irBinarySize);

    // 0 - buildToIrBinary > irBinary
    // 1 - buildSourceCode > genBinary
    // 2 - buildSourceCode > debugDataBinary
    const auto givenCacheBinaryGenHash = cacheMock->cacheBinaryKernelFileHashes[1];
    const auto expectedCacheBinaryGenHash = cacheMock->getCachedFileName(mockOfflineCompiler->getHardwareInfo(),
                                                                         ArrayRef<const char>(mockOfflineCompiler->irBinary, mockOfflineCompiler->irBinarySize),
                                                                         mockOfflineCompiler->options,
                                                                         mockOfflineCompiler->internalOptions,
                                                                         ArrayRef<const char>(), ArrayRef<const char>(),
                                                                         std::string(mockOfflineCompiler->igcFacade->getIgcRevision()),
                                                                         mockOfflineCompiler->igcFacade->getIgcLibSize(),
                                                                         mockOfflineCompiler->igcFacade->getIgcLibMTime());

    EXPECT_EQ(expectedCacheBinaryGenHash, givenCacheBinaryGenHash);
}

TEST(OfflineCompilerTest, WhenParsingCmdLineThenOptionsAreReadCorrectly) {
    std::vector<std::string> argv = {
        "ocloc",
        NEO::CompilerOptions::greaterThan4gbBuffersRequired.data()};

    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = capture.getCapturedStdout();

    std::string internalOptions = mockOfflineCompiler->internalOptions;
    size_t found = internalOptions.find(argv.begin()[1]);
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, GivenUnknownIsaConfigValueWhenInitHardwareInfoThenInvalidDeviceIsReturned) {
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

    auto deviceName = ProductConfigHelper::parseMajorMinorRevisionValue(AOT::UNKNOWN_ISA);
    std::stringstream resString;

    auto retVal = mockOfflineCompiler->initHardwareInfoForProductConfig(deviceName);
    EXPECT_EQ(retVal, OCLOC_INVALID_DEVICE);
}

TEST(OfflineCompilerTest, GivenUnsupportedDeviceConfigWhenInitHardwareInfoThenInvalidDeviceIsReturned) {
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

    auto deviceName = "00.01.02";
    std::stringstream resString;

    auto retVal = mockOfflineCompiler->initHardwareInfoForProductConfig(deviceName);
    EXPECT_EQ(retVal, OCLOC_INVALID_DEVICE);
}

TEST(OfflineCompilerTest, GivenUnsupportedDeviceWhenInitHardwareInfoThenInvalidDeviceIsReturned) {
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();

    auto deviceName = "unk";
    std::stringstream resString;

    StreamCapture capture;
    capture.captureStdout();
    auto retVal = mockOfflineCompiler->initHardwareInfo(deviceName);
    EXPECT_EQ(retVal, OCLOC_INVALID_DEVICE);

    auto output = capture.getCapturedStdout();
    resString << "Could not determine device target: " << deviceName << ".\n";
    EXPECT_STREQ(output.c_str(), resString.str().c_str());
}

TEST(OfflineCompilerTest, givenStatelessToStatefulOptimizationEnabledWhenDebugSettingsAreParsedThenOptimizationStringIsPresent) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.internalOptions;
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, givenStatelessToStatefullOptimizationEnabledWhenDebugSettingsAreParsedThenOptimizationStringIsSetToDefault) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(-1);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.internalOptions;
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, GivenDelimitersWhenGettingStringThenParseIsCorrect) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "copy_buffer_to_buffer.builtin_kernel"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::fseekPtr)> mockFseek(&NEO::IoFunctions::fseekPtr, [](FILE *stream, long int offset, int origin) -> int { return 0; });
    VariableBackup<decltype(NEO::IoFunctions::ftellPtr)> mockFtell(&NEO::IoFunctions::ftellPtr, [](FILE *stream) -> long int {
        std::string kernelSource = R"( 
            /*
            * Copyright (C) 2020-2023 Intel Corporation
            *
            * SPDX-License-Identifier: MIT
            *
            */

            R"===(
            #define ALIGNED4(ptr) __builtin_assume(((size_t)ptr&0b11) == 0)

            __kernel void CopyBufferToBufferBytes(
                const __global uchar* pSrc,
                __global uchar* pDst,
                uint srcOffsetInBytes,
                uint dstOffsetInBytes,
                uint bytesToRead )
            {
                ALIGNED4(pSrc);
                ALIGNED4(pDst);
                pSrc += ( srcOffsetInBytes + get_global_id(0) );
                pDst += ( dstOffsetInBytes + get_global_id(0) );
                pDst[ 0 ] = pSrc[ 0 ];
            }
            )===")";
        std::stringstream fileStream(kernelSource);
        fileStream.seekg(0, std::ios::end);
        return static_cast<long int>(fileStream.tellg());
    });
    VariableBackup<decltype(NEO::IoFunctions::freadPtr)> mockFread(&NEO::IoFunctions::freadPtr, [](void *ptr, size_t size, size_t count, FILE *stream) -> size_t {
        std::string kernelSource = R"( 
            /*
            * Copyright (C) 2020-2023 Intel Corporation
            *
            * SPDX-License-Identifier: MIT
            *
            */

            R"===(
            #define ALIGNED4(ptr) __builtin_assume(((size_t)ptr&0b11) == 0)

            __kernel void CopyBufferToBufferBytes(
                const __global uchar* pSrc,
                __global uchar* pDst,
                uint srcOffsetInBytes,
                uint dstOffsetInBytes,
                uint bytesToRead )
            {
                ALIGNED4(pSrc);
                ALIGNED4(pDst);
                pSrc += ( srcOffsetInBytes + get_global_id(0) );
                pDst += ( dstOffsetInBytes + get_global_id(0) );
                pDst[ 0 ] = pSrc[ 0 ];
            }
            )===")";
        std::stringstream fileStream(kernelSource);
        size_t totalBytes = size * count;
        fileStream.read(static_cast<char *>(ptr), totalBytes);
        return static_cast<size_t>(fileStream.gcount() / size);
    });

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
    EXPECT_EQ(0, strcmp("FillImage1dBuffer", convertToPascalCase("fill_image1d_buffer").c_str()));
    EXPECT_EQ(0, strcmp("VmeBlockMotionEstimateIntel", convertToPascalCase("vme_block_motion_estimate_intel").c_str()));
    EXPECT_EQ(0, strcmp("VmeBlockAdvancedMotionEstimateCheckIntel", convertToPascalCase("vme_block_advanced_motion_estimate_check_intel").c_str()));
    EXPECT_EQ(0, strcmp("VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel", convertToPascalCase("vme_block_advanced_motion_estimate_bidirectional_check_intel").c_str()));
    EXPECT_EQ(0, strcmp("Scheduler", convertToPascalCase("scheduler").c_str()));
    EXPECT_EQ(0, strcmp("", convertToPascalCase("").c_str()));
}

TEST(OfflineCompilerTest, GivenValidParamWhenGettingHardwareInfoThenSuccessIsReturned) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    StreamCapture capture;
    capture.captureStdout();

    EXPECT_EQ(CL_INVALID_DEVICE, mockOfflineCompiler->initHardwareInfo("invalid"));
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(PRODUCT_FAMILY::IGFX_UNKNOWN, mockOfflineCompiler->getHardwareInfo().platform.eProductFamily);
    EXPECT_EQ(CL_SUCCESS, mockOfflineCompiler->initHardwareInfo(gEnvironment->devicePrefix.c_str()));
    EXPECT_NE(PRODUCT_FAMILY::IGFX_UNKNOWN, mockOfflineCompiler->getHardwareInfo().platform.eProductFamily);

    EXPECT_STREQ("Could not determine device target: invalid.\n", output.c_str());
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

TEST_F(OfflineCompilerTests, GivenSourceCodeWhenBuildingThenSuccessIsReturned) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    auto retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, mockOfflineCompiler->genBinary);
    EXPECT_EQ(0u, mockOfflineCompiler->genBinarySize);

    retVal = mockOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, mockOfflineCompiler->genBinary);
    EXPECT_NE(0u, mockOfflineCompiler->genBinarySize);
}

TEST_F(OfflineCompilerTests, givenSpvOnlyOptionPassedWhenCmdLineParsedThenGenerateOnlySpvFile) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-output",
        "myOutputFileName",
        "-spv_only",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
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

TEST_F(OfflineCompilerTests, GivenKernelWhenNoCharAfterKernelSourceThenBuildWithSuccess) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    const char kernelWithNoCharAfterKernel[] = "__kernel void empty(){}";
    Source source{reinterpret_cast<const uint8_t *>(kernelWithNoCharAfterKernel), sizeof(kernelWithNoCharAfterKernel), "emptykernel.cl"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(source);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "emptykernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = mockOfflineCompiler->build();
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
    mockOfflineCompiler.genBinary = new char[1];
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

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.bin");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    EXPECT_EQ("12345678", outputFileIt->second);
}

TEST(OfflineCompilerTest, givenOutputNoSuffixFlagAndOutputFileNameWithExtensionOutWhenWritingOutAllFilesThenBinaryFileDoesNotHaveExtensionBinAdded) {
    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;

    mockOfflineCompiler.outputNoSuffix = true;
    mockOfflineCompiler.outputFile = "some_output_filename.out";
    mockOfflineCompiler.elfBinary = {49, 50, 51, 52, 53, 54, 55, 56}; // ASCII codes of "12345678"

    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.out");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    mockOfflineCompiler.uniqueHelper->interceptedFiles.clear();
    mockOfflineCompiler.outputFile = "some_output_filename.out1";
    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt2 = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.out1.bin");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt2);
}
TEST(OfflineCompilerTest, givenOutputNoSuffixFlagAndOutputFileNameWithExtensionExeWhenWritingOutAllFilesThenBinaryFileDoesNotHaveExtensionBinAdded) {
    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;

    mockOfflineCompiler.outputNoSuffix = true;
    mockOfflineCompiler.outputFile = "some_output_filename.exe";
    mockOfflineCompiler.elfBinary = {49, 50, 51, 52, 53, 54, 55, 56}; // ASCII codes of "12345678"

    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.exe");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    mockOfflineCompiler.uniqueHelper->interceptedFiles.clear();
    mockOfflineCompiler.outputFile = "some_output_filename.exe1";
    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt2 = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.exe1.bin");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt2);
}

TEST(OfflineCompilerTest, givenInputFileNameAndOutputNoSuffixFlagAndEmptyOutputFileNameAndNonEmptyElfContentWhenWritingOutAllFilesThenFileWithTruncatedInputNameIsWritten) {
    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;

    mockOfflineCompiler.outputNoSuffix = true;
    mockOfflineCompiler.inputFile = "/home/important_file.spv";
    mockOfflineCompiler.elfBinary = {49, 50, 51, 52, 53, 54, 55, 56}; // ASCII codes of "12345678"

    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("important_file.bin");
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

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("/home/important/compilation/some_output_filename.bin");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    EXPECT_EQ("12345678", outputFileIt->second);
}

TEST(OfflineCompilerTest, givenBinaryOutputFileWhenWritingOutAllFilesThenOnlyBinaryWithCorrectNameIsCreated) {
    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.interceptCreatedDirs = true;
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;

    mockOfflineCompiler.binaryOutputFile = "some_output_filename.bin";
    mockOfflineCompiler.elfBinary = {49, 50, 51, 52, 53, 54, 55, 56}; // ASCII codes of "12345678"
    mockOfflineCompiler.irBinary = new char[4];
    mockOfflineCompiler.irBinarySize = 4;

    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.bin");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    EXPECT_EQ("12345678", outputFileIt->second);

    const auto outputFileIt2 = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.spv");
    EXPECT_EQ(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt2);
}

TEST(OfflineCompilerTest, givenBinaryOutputFileWithSpirvOnlyWhenWritingOutAllFilesThenOnlyBinaryWithCorrectNameIsCreated) {
    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.interceptCreatedDirs = true;
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;

    mockOfflineCompiler.binaryOutputFile = "some_output_filename.bin";
    mockOfflineCompiler.elfBinary = {0, 0, 0, 0, 0, 0};
    mockOfflineCompiler.irBinary = new char[4];
    mockOfflineCompiler.irBinarySize = 4;
    uint8_t data[] = {49, 50, 51, 52}; // ASCII codes of "1234"
    memcpy(mockOfflineCompiler.irBinary, data, sizeof(data));
    mockOfflineCompiler.onlySpirV = true;

    mockOfflineCompiler.writeOutAllFiles();

    const auto outputFileIt = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.bin");
    ASSERT_NE(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt);

    EXPECT_EQ("1234", outputFileIt->second);

    const auto outputFileIt2 = mockOfflineCompiler.uniqueHelper->interceptedFiles.find("some_output_filename.spv");
    EXPECT_EQ(mockOfflineCompiler.uniqueHelper->interceptedFiles.end(), outputFileIt2);
}

TEST(OfflineCompilerTest, givenLlvmInputOptionPassedWhenCmdLineParsedThenInputFileLlvmIsSetTrue) {
    std::vector<std::string> argv = {
        "ocloc",
        "-llvm_input"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = capture.getCapturedStdout();

    EXPECT_NE(0u, output.size());

    bool llvmFileOption = mockOfflineCompiler->inputFileLlvm();
    EXPECT_TRUE(llvmFileOption);
}

TEST(OfflineCompilerTest, givenDefaultOfflineCompilerObjectWhenNoOptionsAreChangedThenLlvmInputFileIsFalse) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    bool llvmFileOption = mockOfflineCompiler->inputFileLlvm();
    EXPECT_FALSE(llvmFileOption);
}

TEST(OfflineCompilerTest, givenSpirvInputOptionPassedWhenCmdLineParsedThenInputFileSpirvIsSetTrue) {
    std::vector<std::string> argv = {"ocloc", "-spirv_input"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = capture.getCapturedStdout();
    EXPECT_NE(0u, output.size());

    EXPECT_TRUE(mockOfflineCompiler->inputFileSpirV());
}

TEST(OfflineCompilerTest, givenDefaultOfflineCompilerObjectWhenNoOptionsAreChangedThenSpirvInputFileIsFalse) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    EXPECT_FALSE(mockOfflineCompiler->inputFileSpirV());
}

TEST_F(OfflineCompilerTests, givenSpirvRepresentationInputWhenBuildSourceCodeIsCalledThenProperTranslationContextIsUsed) {
    MockOfflineCompiler mockOfflineCompiler;

    Source source{reinterpret_cast<const uint8_t *>(spirvMagic.data()), spirvMagic.size(), "some_file.spv"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler.argHelper)->inputs.push_back(source);
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "some_file.spv",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spirv_input"};

    struct StdoutCaptureRAII {
        StreamCapture capture;
        StdoutCaptureRAII() { capture.captureStdout(); }
        ~StdoutCaptureRAII() {
            auto output = capture.getCapturedStdout();
            if (HasFatalFailure()) {
                printf("%s", output.c_str());
            }
        }
    } stdoutCaptureRAII;
    auto retVal = mockOfflineCompiler.initialize(argv.size(), argv);
    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.mockIgcFacade->igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxTagOCL>(mockIgcOclDeviceCtx);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = mockOfflineCompiler.build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1U, mockIgcOclDeviceCtx->requestedTranslationCtxs.size());
    NEO::MockIgcOclDeviceCtx::TranslationOpT expectedTranslation = {IGC::CodeType::spirV, IGC::CodeType::oclGenBin};
    ASSERT_EQ(expectedTranslation, mockIgcOclDeviceCtx->requestedTranslationCtxs[0]);
}

TEST_F(OfflineCompilerTests, givenLlvmBcRepresentationInputWhenBuildSourceCodeIsCalledThenProperTranslationContextIsUsed) {
    MockOfflineCompiler mockOfflineCompiler;

    Source source{reinterpret_cast<const uint8_t *>(llvmBcMagic.data()), llvmBcMagic.size(), "some_file.bc"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler.argHelper)->inputs.push_back(source);
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "some_file.bc",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-llvm_input",
    };

    struct StdoutCaptureRAII {
        StreamCapture capture;
        StdoutCaptureRAII() { capture.captureStdout(); }
        ~StdoutCaptureRAII() {
            auto output = capture.getCapturedStdout();
            if (HasFatalFailure()) {
                printf("%s", output.c_str());
            }
        }
    } stdoutCaptureRAII;
    auto retVal = mockOfflineCompiler.initialize(argv.size(), argv);
    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.mockIgcFacade->igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxTagOCL>(mockIgcOclDeviceCtx);
    ASSERT_EQ(CL_SUCCESS, retVal);

    mockIgcOclDeviceCtx->requestedTranslationCtxs.clear();
    retVal = mockOfflineCompiler.build();

    ASSERT_EQ(mockOfflineCompiler.irBinarySize, mockOfflineCompiler.sourceCode.size());
    EXPECT_EQ(0, memcmp(mockOfflineCompiler.irBinary, mockOfflineCompiler.sourceCode.data(), mockOfflineCompiler.sourceCode.size()));
    EXPECT_NE(IGC::CodeType::spirV, mockOfflineCompiler.intermediateRepresentation);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1U, mockIgcOclDeviceCtx->requestedTranslationCtxs.size());
    NEO::MockIgcOclDeviceCtx::TranslationOpT expectedTranslation = {IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin};
    ASSERT_EQ(expectedTranslation, mockIgcOclDeviceCtx->requestedTranslationCtxs[0]);
}

TEST_F(OfflineCompilerTests, givenUseLlvmBcFlagWhenBuildingIrBinaryThenProperTranslationContextIsUsed) {
    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clFiles + "emptykernel.cl",
        "-llvm_input",
        "-llvm_bc",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    const auto initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    ASSERT_EQ(CL_SUCCESS, initResult);

    auto mockFclOclDeviceCtx = new NEO::MockFclOclDeviceCtx();
    mockOfflineCompiler.mockFclFacade->fclDeviceCtx = CIF::RAII::Pack<IGC::FclOclDeviceCtxTagOCL>(mockFclOclDeviceCtx);

    const auto buildResult = mockOfflineCompiler.buildToIrBinary();
    EXPECT_EQ(CL_SUCCESS, buildResult);

    ASSERT_EQ(1U, mockFclOclDeviceCtx->requestedTranslationCtxs.size());

    NEO::MockIgcOclDeviceCtx::TranslationOpT expectedTranslation = {IGC::CodeType::oclC, IGC::CodeType::llvmBc};
    EXPECT_EQ(expectedTranslation, mockFclOclDeviceCtx->requestedTranslationCtxs[0]);
}

TEST_F(OfflineCompilerTests, givenBinaryInputThenDontTruncateSourceAtFirstZero) {
    std::vector<std::string> argvLlvm = {"ocloc", "-llvm_input", "-file", "binary_with_zeroes", "-qq",
                                         "-device", gEnvironment->devicePrefix.c_str()};
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    uint8_t binaryWithZeros[64]{};
    Source source{binaryWithZeros, sizeof(binaryWithZeros), "binary_with_zeroes"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(source);
    mockOfflineCompiler->initialize(argvLlvm.size(), argvLlvm);
    mockOfflineCompiler->build();
    EXPECT_LT(0U, mockOfflineCompiler->sourceCode.size());

    std::vector<std::string> argvSpirV = {"ocloc", "-spirv_input", "-file", "binary_with_zeroes", "-qq",
                                          "-device", gEnvironment->devicePrefix.c_str()};
    mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(source);
    mockOfflineCompiler->initialize(argvSpirV.size(), argvSpirV);
    mockOfflineCompiler->build();
    EXPECT_LT(0U, mockOfflineCompiler->sourceCode.size());
}

TEST(OfflineCompilerTest, givenSpirvInputFileWhenCmdLineHasOptionsThenCorrectOptionsArePassedToCompiler) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    char data[] = {1, 2, 3, 4, 5, 6, 7, 8};
    MockCompilerDebugVars igcDebugVars(gEnvironment->igcDebugVars);
    igcDebugVars.binaryToReturn = data;
    igcDebugVars.binaryToReturnSize = sizeof(data);

    NEO::setIgcDebugVars(igcDebugVars);

    MockOfflineCompiler mockOfflineCompiler;
    Source source{reinterpret_cast<const uint8_t *>(spirvMagic.data()), spirvMagic.size(), "some_file.spv"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler.argHelper)->inputs.push_back(source);
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "some_file.spv",
        "-spirv_input",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "test_options_passed", "-qq"};

    auto retVal = mockOfflineCompiler.initialize(argv.size(), argv);
    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.mockIgcFacade->igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxTagOCL>(mockIgcOclDeviceCtx);
    ASSERT_EQ(CL_SUCCESS, retVal);

    retVal = mockOfflineCompiler.build();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_STREQ("test_options_passed", mockOfflineCompiler.options.c_str());
    NEO::setIgcDebugVars(gEnvironment->igcDebugVars);
}

TEST_F(OfflineCompilerTests, givenOutputFileOptionWhenSourceIsCompiledThenOutputFileHasCorrectName) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-output",
        "myOutputFileName",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bc") || compilerOutputExists("myOutputFileName", "spv"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));

    retVal = mockOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bc") || compilerOutputExists("myOutputFileName", "spv"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));
}

TEST_F(OfflineCompilerTests, givenDebugDataAvailableWhenSourceIsBuiltThenDebugDataFileIsCreated) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
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

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
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
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));
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

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = capture.getCapturedStdout();

    EXPECT_NE(0u, output.size());

    std::string internalOptions = mockOfflineCompiler->internalOptions;
    EXPECT_TRUE(hasSubstr(internalOptions, std::string("myInternalOptions")));
}

TEST(OfflineCompilerTest, givenOptionsWhenCmdLineParsedThenOptionsAreAppendedToOptionsString) {
    std::vector<std::string> argv = {
        "ocloc",
        "-options",
        "options1",
        "-options",
        "options2"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = capture.getCapturedStdout();

    EXPECT_NE(0u, output.size());

    std::string options = mockOfflineCompiler->options;
    EXPECT_TRUE(hasSubstr(options, std::string("options1")));
    EXPECT_TRUE(hasSubstr(options, std::string("options2")));
}

TEST(OfflineCompilerTest, givenDeviceOptionsForCompiledDeviceWhenCmdLineParsedThenDeviceOptionsAreAppendedToOptionsString) {
    std::vector<std::string> argv = {
        "ocloc",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "options",
        "-device_options",
        gEnvironment->devicePrefix.c_str(),
        "devOptions"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->parseCommandLine(argv.size(), argv);

    auto perDeviceOptions = mockOfflineCompiler->perDeviceOptions;
    EXPECT_TRUE(hasSubstr(perDeviceOptions[gEnvironment->devicePrefix.c_str()], std::string("devOptions")));

    std::string options = mockOfflineCompiler->options;
    EXPECT_TRUE(hasSubstr(options, std::string("options")));
    EXPECT_TRUE(hasSubstr(options, std::string("devOptions")));
}

TEST_F(OfflineCompilerTests, givenDeviceOptionsWithDeprecatedDeviceAcronymWhenCmdLineParsedThenDeviceNameIsRecognized) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto deprecatedAcronyms = mockOfflineCompiler->argHelper->productConfigHelper->getDeprecatedAcronyms();
    if (deprecatedAcronyms.empty()) {
        GTEST_SKIP();
    }

    const auto deviceName = deprecatedAcronyms[0].str();

    std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        deviceName.c_str(),
        "-options",
        "options",
        "-device_options",
        deviceName.c_str(),
        "devOptions"};

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    const auto result = mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(0u, output.size());
    EXPECT_NE(OCLOC_INVALID_COMMAND_LINE, result);

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error: Invalid device acronym passed to -device_options: " << deviceName;
    EXPECT_FALSE(hasSubstr(output, expectedErrorMessage.str()));
}

TEST_F(OfflineCompilerTests, givenUnknownDeviceAcronymInDeviceOptionsWhenParsingCommandLineThenErrorLogIsPrintedAndFailureIsReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "options",
        "-device_options",
        "unknownDeviceName1",
        "devOptions1",
        "-device_options",
        gEnvironment->devicePrefix.c_str(),
        "devOptions2",
        "-device_options",
        "unknownDeviceName2",
        "devOptions2"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    const std::string output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, result);

    const std::string expectedErrorMessage1{"Error: Invalid device acronym passed to -device_options: unknownDeviceName1\n"};
    const std::string expectedErrorMessage2{"Error: Invalid device acronym passed to -device_options: unknownDeviceName2\n"};
    EXPECT_TRUE(hasSubstr(output, expectedErrorMessage1));
    EXPECT_TRUE(hasSubstr(output, expectedErrorMessage2));
}

TEST(OfflineCompilerTest, givenDeviceOptionsInWrongFormatWhenCmdLineParsedThenDeviceOptionsAreNotAppendedToOptionsString) {
    std::vector<std::string> argv = {
        "ocloc",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "options",
        "-device_options",
        "devOptions"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = capture.getCapturedStdout();

    EXPECT_NE(0u, output.size());

    auto perDeviceOptions = mockOfflineCompiler->perDeviceOptions;
    EXPECT_FALSE(hasSubstr(perDeviceOptions[gEnvironment->devicePrefix.c_str()], std::string("devOptions")));

    std::string options = mockOfflineCompiler->options;
    EXPECT_TRUE(hasSubstr(options, std::string("options")));
    EXPECT_FALSE(hasSubstr(options, std::string("devOptions")));
}

TEST(OfflineCompilerTest, givenMultipleDeviceOptionsWhenCmdLineParsedThenDeviceOptionsAreAppendedToOptionsString) {
    std::vector<std::string> argv = {
        "ocloc",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "options",
        "-device_options",
        gEnvironment->devicePrefix.c_str(),
        "devOptions1",
        "-device_options",
        gEnvironment->devicePrefix.c_str(),
        "devOptions2"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->parseCommandLine(argv.size(), argv);

    auto perDeviceOptions = mockOfflineCompiler->perDeviceOptions;
    EXPECT_TRUE(hasSubstr(perDeviceOptions[gEnvironment->devicePrefix.c_str()], std::string("devOptions1")));
    EXPECT_TRUE(hasSubstr(perDeviceOptions[gEnvironment->devicePrefix.c_str()], std::string("devOptions2")));

    std::string options = mockOfflineCompiler->options;
    EXPECT_TRUE(hasSubstr(options, std::string("options")));
    EXPECT_TRUE(hasSubstr(options, std::string("devOptions1")));
    EXPECT_TRUE(hasSubstr(options, std::string("devOptions2")));
}

TEST(OfflineCompilerTest, givenMultipleDeviceOptionsForCompiledDeviceAndDeviceOptionsNotForCompiledDeviceWhenCmdLineParsedThenDeviceOptionsOnlyForCompiledDeviceAreAppendedToOptionsString) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto allEnabledDeviceAcronyms = mockOfflineCompiler->argHelper->productConfigHelper->getRepresentativeProductAcronyms();

    if (allEnabledDeviceAcronyms.empty()) {
        GTEST_SKIP();
    }

    std::string notCompiledDevice;
    for (const auto &deviceAcronym : allEnabledDeviceAcronyms) {
        if (deviceAcronym.str() != gEnvironment->devicePrefix.c_str()) {
            notCompiledDevice = deviceAcronym.str();
            break;
        }
    }

    if (notCompiledDevice == "") {
        GTEST_SKIP();
    }

    std::vector<std::string> argv = {
        "ocloc",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-device_options",
        gEnvironment->devicePrefix.c_str(),
        "devOptions1",
        "-device_options",
        notCompiledDevice,
        "devOptions2",
        "-options",
        "options1",
        "-device_options",
        gEnvironment->devicePrefix.c_str(),
        "devOptions3",
        "-options",
        "options2"};

    mockOfflineCompiler->parseCommandLine(argv.size(), argv);

    auto perDeviceOptions = mockOfflineCompiler->perDeviceOptions;
    EXPECT_TRUE(hasSubstr(perDeviceOptions[notCompiledDevice], std::string("devOptions2")));
    EXPECT_TRUE(hasSubstr(perDeviceOptions[gEnvironment->devicePrefix.c_str()], std::string("devOptions1")));
    EXPECT_TRUE(hasSubstr(perDeviceOptions[gEnvironment->devicePrefix.c_str()], std::string("devOptions3")));

    std::string options = mockOfflineCompiler->options;
    EXPECT_TRUE(hasSubstr(options, std::string("options1")));
    EXPECT_TRUE(hasSubstr(options, std::string("options2")));
    EXPECT_TRUE(hasSubstr(options, std::string("devOptions1")));
    EXPECT_TRUE(hasSubstr(options, std::string("devOptions3")));
    EXPECT_FALSE(hasSubstr(options, std::string("devOptions2")));
}

TEST(OfflineCompilerTest, givenDeviceOptionsForMultipleDevicesSeparatedByCommasWhenCmdLineParsedThenDeviceOptionsAreParsedCorrectly) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto allEnabledDeviceAcronyms = mockOfflineCompiler->argHelper->productConfigHelper->getRepresentativeProductAcronyms();
    if (allEnabledDeviceAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    auto product = allEnabledDeviceAcronyms[0].str();
    auto productForDeviceOptions0 = allEnabledDeviceAcronyms[0].str();
    auto productForDeviceOptions1 = allEnabledDeviceAcronyms[1].str();

    std::stringstream productsForDeviceOptions;
    productsForDeviceOptions << productForDeviceOptions0 << ","
                             << productForDeviceOptions1;

    std::vector<std::string> argv = {
        "ocloc",
        "-device",
        product.c_str(),
        "-device_options",
        productsForDeviceOptions.str().c_str(),
        "devOptions1",
        "-options",
        "options1",
        "-device_options",
        productForDeviceOptions1.c_str(),
        "devOptions2"};

    mockOfflineCompiler->parseCommandLine(argv.size(), argv);

    auto perDeviceOptions = mockOfflineCompiler->perDeviceOptions;
    EXPECT_TRUE(hasSubstr(perDeviceOptions[productForDeviceOptions0], std::string("devOptions1")));
    EXPECT_FALSE(hasSubstr(perDeviceOptions[productForDeviceOptions0], std::string("devOptions2")));

    EXPECT_TRUE(hasSubstr(perDeviceOptions[productForDeviceOptions1], std::string("devOptions1")));
    EXPECT_TRUE(hasSubstr(perDeviceOptions[productForDeviceOptions1], std::string("devOptions2")));

    std::string options = mockOfflineCompiler->options;
    EXPECT_TRUE(hasSubstr(options, std::string("options1")));
    EXPECT_TRUE(hasSubstr(options, std::string("devOptions1")));
    EXPECT_FALSE(hasSubstr(options, std::string("devOptions2")));
}

TEST(OfflineCompilerTest, givenDeviceOptionsForMultipleDevicesSeparatedByCommasWithSpacesAfterCommasWhenCmdLineParsedThenErrorLogIsPrintedAndFailureIsReturned) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto allEnabledDeviceAcronyms = mockOfflineCompiler->argHelper->productConfigHelper->getRepresentativeProductAcronyms();
    if (allEnabledDeviceAcronyms.size() < 3) {
        GTEST_SKIP();
    }

    auto product = allEnabledDeviceAcronyms[0].str();
    auto productForDeviceOptions0 = allEnabledDeviceAcronyms[0].str();
    auto productForDeviceOptions1 = allEnabledDeviceAcronyms[1].str();

    std::stringstream productsForDeviceOptions;
    productsForDeviceOptions << productForDeviceOptions0 << ", " << productForDeviceOptions1;

    std::vector<std::string> argv = {
        "ocloc",
        "-device",
        product.c_str(),
        "-device_options",
        productsForDeviceOptions.str().c_str(),
        "devOptions1",
        "-options",
        "options1",
        "-device_options",
        productForDeviceOptions1.c_str(),
        "devOptions2"};

    StreamCapture capture;
    capture.captureStdout();
    const auto result = mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    const std::string output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, result);
    EXPECT_NE(0u, output.size());

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error: Invalid device acronym passed to -device_options: "
                         << " " << productForDeviceOptions1 << "\n";

    EXPECT_TRUE(hasSubstr(output, expectedErrorMessage.str()));
}

TEST_F(OfflineCompilerTests, givenDashOOptionWhenCmdLineParsedThenBinaryOutputNameIsSet) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-o",
        "nameOfFile.bin",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;

    StreamCapture capture;
    capture.captureStdout();
    auto retVal = mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_EQ(0u, output.size());

    EXPECT_EQ("nameOfFile.bin", mockOfflineCompiler->binaryOutputFile);
}

TEST_F(OfflineCompilerTests, givenDashOAndOtherInvalidOptionsWhenCmdLineParsedThenErrorReturned) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-o",
        "nameOfFile.bin",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "empty"};

    std::vector<std::string> options = {
        "-gen_file",
        "-cpp_file",
        "-output_no_suffix"};

    for (const auto &op : options) {
        argv[argv.size() - 1] = op;
        auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
        ASSERT_NE(nullptr, mockOfflineCompiler);

        mockOfflineCompiler->uniqueHelper->filesMap = filesMap;

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = mockOfflineCompiler->parseCommandLine(argv.size(), argv);
        std::string output = capture.getCapturedStdout();

        EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
        EXPECT_NE(0u, output.size());

        EXPECT_TRUE(hasSubstr(output, std::string("Error: options: -gen_file/-cpp_file/-output_no_suffix/-output cannot be used with -o\n")));
    }

    std::vector<std::string> argv2 = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-o",
        "nameOfFile.bin",
        "-output",
        "abc",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;

    StreamCapture capture;
    capture.captureStdout();
    auto retVal = mockOfflineCompiler->parseCommandLine(argv2.size(), argv2);
    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
    EXPECT_NE(0u, output.size());

    EXPECT_TRUE(hasSubstr(output, std::string("Error: options: -gen_file/-cpp_file/-output_no_suffix/-output cannot be used with -o\n")));
}

TEST(OfflineCompilerTest, givenInputOptionsAndInternalOptionsFilesWhenOfflineCompilerIsInitializedThenCorrectOptionsAreSetAndRemainAfterBuild) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * {
        std::filesystem::path filePath = filename;
        std::string fileNameWithExtension = filePath.filename().string();

        std::vector<std::string> expectedtedFiles = {
            "some_kernel.cl"};

        auto itr = std::find(expectedtedFiles.begin(), expectedtedFiles.end(), std::string(fileNameWithExtension));
        if (itr != expectedtedFiles.end()) {
            return reinterpret_cast<FILE *>(0x40);
        }
        return NULL;
    });
    VariableBackup<decltype(NEO::IoFunctions::fclosePtr)> mockFclose(&NEO::IoFunctions::fclosePtr, [](FILE *stream) -> int { return 0; });

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    const char kernelSource[] = R"===(
/*
* Copyright (C) 2018-2021 Intel Corporation
*
* SPDX-License-Identifier: MIT
*
*/

__kernel void shouldfail(global ushort *dst) {
    // idx and dummy are not defined, compiler should fail the build.
    dst[idx] = dummy;
}
)===";
    Source source{reinterpret_cast<const uint8_t *>(kernelSource), sizeof(kernelSource), "kernel.cl"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(source);

    const char internalOptions[] = R"===(
/*
* Copyright (C) 2018-2021 Intel Corporation
*
* SPDX-License-Identifier: MIT
*
*/

-shouldfailInternalOptions
)===";
    Source internalOptionSource{reinterpret_cast<const uint8_t *>(internalOptions), sizeof(internalOptions), "kernel_internal_options.txt"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(internalOptionSource);

    const char options[] = R"===(
/*
* Copyright (C) 2018-2021 Intel Corporation
*
* SPDX-License-Identifier: MIT
*
*/

-shouldfailOptions
)===";
    Source optionSource{reinterpret_cast<const uint8_t *>(options), sizeof(options), "kernel_options.txt"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(optionSource);
    {
        std::vector<std::string> args = {
            "ocloc",
            "-q",
            "-file",
            "kernel.cl",
            "-device",
            gEnvironment->devicePrefix.c_str()};

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = Ocloc::Commands::compile(mockOfflineCompiler->argHelper, args);
        EXPECT_NE(retVal, OCLOC_SUCCESS);
        std::string output = capture.getCapturedStdout();
        EXPECT_FALSE(output.find("Building with options:\n"
                                 "-shouldfailOptions") != std::string::npos);

        EXPECT_FALSE(output.find("Building with internal options:\n"
                                 "-shouldfailInternalOptions") != std::string::npos);

        EXPECT_TRUE(output.find("Compiling options read from file were:\n"
                                "-shouldfailOptions") != std::string::npos);

        EXPECT_TRUE(output.find("Internal options read from file were:\n"
                                "-shouldfailInternalOptions") != std::string::npos);
    }
    {
        std::vector<std::string> args = {
            "ocloc",
            "-file",
            "kernel.cl",
            "-device",
            gEnvironment->devicePrefix.c_str()};

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = Ocloc::Commands::compile(mockOfflineCompiler->argHelper, args);
        EXPECT_NE(retVal, OCLOC_SUCCESS);
        std::string output = capture.getCapturedStdout();
        EXPECT_TRUE(output.find("Building with options:\n"
                                "-shouldfailOptions") != std::string::npos);

        EXPECT_TRUE(output.find("Building with internal options:\n"
                                "-shouldfailInternalOptions") != std::string::npos);

        EXPECT_TRUE(output.find("Compiling options read from file were:\n"
                                "-shouldfailOptions") != std::string::npos);

        EXPECT_TRUE(output.find("Internal options read from file were:\n"
                                "-shouldfailInternalOptions") != std::string::npos);
    }
    {
        std::vector<std::string> args = {
            "ocloc",
            "-file",
            "kernel.cl",
            "-options",
            "-invalid_options",
            "-internal_options",
            "-invalid_internal_options",
            "-device",
            gEnvironment->devicePrefix.c_str()};

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = Ocloc::Commands::compile(mockOfflineCompiler->argHelper, args);
        EXPECT_NE(retVal, OCLOC_SUCCESS);
        std::string output = capture.getCapturedStdout();
        EXPECT_FALSE(output.find("Building with options:\n"
                                 "-invalid_options") != std::string::npos);

        EXPECT_FALSE(output.find("Building with internal options:\n"
                                 "-invalid_internal_options") != std::string::npos);

        EXPECT_FALSE(output.find("Compiling options read from file were:") != std::string::npos);

        EXPECT_FALSE(output.find("Internal options read from file were:") != std::string::npos);
    }
}

TEST_F(OfflineCompilerTests, givenInputOptionsAndOclockOptionsFileWithForceStosOptWhenOfflineCompilerIsInitializedThenCompilerOptionGreaterThan4gbBuffersRequiredIsNotApplied) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    mockOfflineCompiler->uniqueHelper->filesMap["some_kernel_ocloc_options.txt"] = "-force_stos_opt";

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
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
    if (!mockOfflineCompiler->compilerProductHelper->isForceToStatelessRequired()) {
        GTEST_SKIP();
    }
    runTest();
}

class MockCompilerProductHelperForceStateless : public MockCompilerProductHelper {
  public:
    bool isForceToStatelessRequired() const override {
        return true;
    }
};

HWTEST2_F(OfflineCompilerStatelessToStatefulTests, givenMockWhenAppendExtraInternalOptionsThenInternalOptionsAreCorrect, MatchAny) {
    auto backup = std::unique_ptr<CompilerProductHelper>(new MockCompilerProductHelperForceStateless());
    this->mockOfflineCompiler->compilerProductHelper.swap(backup);

    runTest();
    this->mockOfflineCompiler->compilerProductHelper.swap(backup);
}

struct OfflineCompilerBindlessOptionsTests : public ::testing::Test {
    void SetUp() override {
        mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
        mockOfflineCompiler->deviceName = gEnvironment->devicePrefix;
        mockOfflineCompiler->initHardwareInfo(mockOfflineCompiler->deviceName);
        mockOfflineCompiler->internalOptions.clear();

        releaseHelper = std::make_unique<MockReleaseHelper>();
    }

    std::unique_ptr<MockOfflineCompiler> mockOfflineCompiler;
    std::unique_ptr<MockReleaseHelper> releaseHelper;
};

TEST_F(OfflineCompilerBindlessOptionsTests, givenBindlessAddressingEnabledWhenAppendExtraInternalOptionsThenBindlessModeOptionsAreAddedToInternalOptions) {
    releaseHelper->isBindlessAddressingDisabledResult = false;
    mockOfflineCompiler->releaseHelper = std::move(releaseHelper);

    auto internalOptions = mockOfflineCompiler->internalOptions;
    mockOfflineCompiler->appendExtraInternalOptions(internalOptions);

    EXPECT_NE(std::string::npos, internalOptions.find(NEO::CompilerOptions::bindlessMode.data()));
}

TEST_F(OfflineCompilerBindlessOptionsTests, givenBindlessNotEnabledAddrModeSetWhenAppendExtraInternalOptionsThenBindlessModeOptionsAreNotAddedToInternalOptions) {
    releaseHelper->isBindlessAddressingDisabledResult = false;
    mockOfflineCompiler->releaseHelper = std::move(releaseHelper);
    mockOfflineCompiler->addressingMode = "bindful";

    auto internalOptions = mockOfflineCompiler->internalOptions;
    mockOfflineCompiler->appendExtraInternalOptions(internalOptions);

    EXPECT_EQ(std::string::npos, internalOptions.find(NEO::CompilerOptions::bindlessMode.data()));
}

TEST_F(OfflineCompilerBindlessOptionsTests, givenBindlessAddressingDisabledWhenAppendExtraInternalOptionsThenBindlessModeOptionsAreNotAddedToInternalOptions) {
    releaseHelper->isBindlessAddressingDisabledResult = true;
    mockOfflineCompiler->releaseHelper = std::move(releaseHelper);

    auto internalOptions = mockOfflineCompiler->internalOptions;
    mockOfflineCompiler->appendExtraInternalOptions(internalOptions);

    EXPECT_EQ(std::string::npos, internalOptions.find(NEO::CompilerOptions::bindlessMode.data()));
}

TEST_F(OfflineCompilerBindlessOptionsTests, givenNullReleaseHelperWhenAppendExtraInternalOptionsThenBindlessModeOptionsAreNotAddedToInternalOptions) {
    mockOfflineCompiler->releaseHelper.reset();

    ASSERT_EQ(nullptr, mockOfflineCompiler->releaseHelper);

    auto internalOptions = mockOfflineCompiler->internalOptions;
    mockOfflineCompiler->appendExtraInternalOptions(internalOptions);

    EXPECT_EQ(std::string::npos, internalOptions.find(NEO::CompilerOptions::bindlessMode.data()));
}

TEST(OfflineCompilerTest, givenNonExistingFilenameWhenUsedToReadOptionsThenReadOptionsFromFileReturnsFalse) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

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
    compiler.intermediateRepresentation = IGC::CodeType::spirV;
    compiler.outputDirectory = "d";
    std::string path = compiler.generateFilePathForIr("a");
    EXPECT_STREQ("d/a.spv", path.c_str());
}

TEST(OfflineCompilerTest, givenLlvmBcPathWhenGenerateFilePathForIrIsCalledThenProperExtensionIsReturned) {
    MockOfflineCompiler compiler;
    compiler.intermediateRepresentation = IGC::CodeType::llvmBc;
    compiler.outputDirectory = "d";
    std::string path = compiler.generateFilePathForIr("a");
    EXPECT_STREQ("d/a.bc", path.c_str());
}

TEST(OfflineCompilerTest, givenLlvmTextPathWhenGenerateFilePathForIrIsCalledThenProperExtensionIsReturned) {
    MockOfflineCompiler compiler;
    compiler.intermediateRepresentation = IGC::CodeType::llvmLl;
    compiler.outputDirectory = "d";
    std::string path = compiler.generateFilePathForIr("a");
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

TEST_F(OfflineCompilerTests, givenCompilerWhenBuildSourceCodeFailsThenGenerateElfBinaryAndWriteOutAllFilesAreCalled) {
    MockOfflineCompiler compiler;
    compiler.overrideBuildSourceCodeStatus = true;
    compiler.uniqueHelper->filesMap = filesMap;

    auto expectedError = OCLOC_BUILD_PROGRAM_FAILURE;
    compiler.buildSourceCodeStatus = expectedError;

    EXPECT_EQ(0u, compiler.generateElfBinaryCalled);
    EXPECT_EQ(0u, compiler.writeOutAllFilesCalled);

    compiler.inputFile = clCopybufferFilename.c_str();
    auto status = compiler.build();
    EXPECT_EQ(expectedError, status);

    EXPECT_EQ(1u, compiler.generateElfBinaryCalled);
    EXPECT_EQ(1u, compiler.writeOutAllFilesCalled);
}

TEST(OfflineCompilerTest, givenDeviceSpecificKernelFileWhenCompilerIsInitializedThenOptionsAreReadFromFile) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    auto kernelForSpecificDevice = "__kernel void empty(){}";
    Source source{reinterpret_cast<const uint8_t *>(kernelForSpecificDevice), sizeof(kernelForSpecificDevice), "emptykernel.cl"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(source);

    const char options[] = R"===(
/*
* Copyright (C) 2024 Intel Corporation
*
* SPDX-License-Identifier: MIT
*
*/

-cl-opt-disable -DDEF_WAS_SPECIFIED=1 -DARGS=", const __global int *arg1, float arg2, const __global int *arg3, float arg4"
)===";
    Source optionsSource{reinterpret_cast<const uint8_t *>(options), sizeof(options), "emptykernel_options.txt"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(optionsSource);

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        "emptykernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_STREQ("-cl-opt-disable -DDEF_WAS_SPECIFIED=1 -DARGS=\", const __global int *arg1, float arg2, const __global int *arg3, float arg4\"", mockOfflineCompiler->options.c_str());
}

TEST_F(OfflineCompilerTests, givenHexadecimalRevisionIdWhenCompilerIsInitializedThenPassItToHwInfo) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;
    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-revision_id",
        "0x11"};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, 17);
}

TEST_F(OfflineCompilerTests, givenDebugVariableSetWhenInitializingThenOverrideRevision) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.OverrideRevision.set(123);

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-revision_id",
        "0x11"};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, 123);
}

TEST_F(OfflineCompilerTests, givenDecimalRevisionIdWhenCompilerIsInitializedThenPassItToHwInfo) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-revision_id",
        "17"};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, 17);
}

TEST_F(OfflineCompilerTests, givenNoRevisionIdWhenCompilerIsInitializedThenHwInfoHasDefaultRevId) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    mockOfflineCompiler->initHardwareInfo(gEnvironment->devicePrefix.c_str());
    auto revId = mockOfflineCompiler->hwInfo.platform.usRevId;
    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_EQ(mockOfflineCompiler->hwInfo.platform.usRevId, revId);
}

TEST(OfflineCompilerTest, givenEmptyDeviceNameWhenInitializingHardwareInfoThenInvalidDeviceIsReturned) {
    MockOfflineCompiler mockOfflineCompiler{};

    const std::string emptyDeviceName{};
    const auto result = mockOfflineCompiler.initHardwareInfo(emptyDeviceName);

    EXPECT_EQ(OCLOC_INVALID_DEVICE, result);
}

TEST_F(OfflineCompilerTests, whenDeviceIsSpecifiedThenDefaultConfigFromTheDeviceIsUsed) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

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

TEST_F(OfflineCompilerTests, whenDetectedPotentialInputTypeMismatchThenEmitsWarning) {
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
        ocloc.uniqueHelper->filesMap = filesMap;

        std::vector<std::string> argv = {
            "ocloc",
            "-q",
            "-file",
            clCopybufferFilename.c_str(),
            "-device",
            gEnvironment->devicePrefix.c_str()};

        ocloc.initHardwareInfo(gEnvironment->devicePrefix.c_str());
        int retVal = ocloc.initialize(argv.size(), argv);
        ASSERT_EQ(0, retVal);

        int caseNum = 0;
        for (auto &c : cases) {

            StreamCapture capture;
            capture.captureStdout();

            ocloc.inputCodeType = IGC::CodeType::oclC;
            if (c.isLlvm) {
                ocloc.inputCodeType = IGC::CodeType::llvmBc;
            }
            if (c.isSpirv) {
                ocloc.inputCodeType = IGC::CodeType::spirV;
            }
            ocloc.inputFile = "src";
            ocloc.uniqueHelper->filesMap["src"] = c.input;
            ocloc.uniqueHelper->callBaseFileExists = false;
            ocloc.uniqueHelper->callBaseLoadDataFromFile = false;
            ocloc.build();
            auto log = ocloc.argHelper->getPrinterRef().getLog().str();
            ocloc.clearLog();

            std::string output = capture.getCapturedStdout();

            if (c.expectedWarning.empty()) {
                for (auto &w : allWarnings) {
                    EXPECT_FALSE(hasSubstr(log, w)) << " Case : " << caseNum;
                }
            } else {
                EXPECT_TRUE(hasSubstr(log, c.expectedWarning)) << " Case : " << caseNum << " : " << c.expectedWarning << ". Got : " << log;
                EXPECT_STREQ(log.c_str(), output.c_str());
            }
            caseNum++;
        }
    }
}

TEST_F(OfflineCompilerTests, givenCommandLineWithoutDeviceWhenCompilingToSpirvThenSucceedsButUsesEmptyExtensionString) {
    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-spv_only"};

    int retVal = ocloc.initialize(argv.size(), argv);
    ASSERT_EQ(0, retVal);
    retVal = ocloc.build();
    EXPECT_EQ(0, retVal);
    EXPECT_TRUE(hasSubstr(ocloc.internalOptions, "-ocl-version=300"
                                                 " -cl-ext=-all,+cl_khr_3d_image_writes,+__opencl_c_3d_image_writes,+__opencl_c_images"));
    EXPECT_TRUE(hasSubstr(ocloc.internalOptions, " -D__IMAGE_SUPPORT__=1"));
}

TEST_F(OfflineCompilerTests, givenDeviceAndInternalOptionsOptionWhenCompilingToSpirvThenInternalOptionsAreSetCorrectly) {
    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
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

TEST_F(OfflineCompilerTests, givenNoDeviceAndInternalOptionsOptionWhenCompilingToSpirvThenInternalOptionsAreSetCorrectly) {
    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-internal_options",
        "-cl-ext=+custom_param",
        "-spv_only"};

    int retVal = ocloc.initialize(argv.size(), argv);
    ASSERT_EQ(0, retVal);
    retVal = ocloc.build();
    EXPECT_EQ(0, retVal);
    EXPECT_TRUE(hasSubstr(ocloc.internalOptions, "-ocl-version=300"
                                                 " -cl-ext=-all,+cl_khr_3d_image_writes,+__opencl_c_3d_image_writes,+__opencl_c_images"
                                                 " -cl-ext=+custom_param"));
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

TEST_F(OfflineCompilerTests, givenSpirvInputThenDontGenerateSpirvFile) {
    VariableBackup<decltype(NEO::IoFunctions::mkdirPtr)> mockCreateDir(&NEO::IoFunctions::mkdirPtr, [](const char *path) -> int { return 0; });

    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    Source source{reinterpret_cast<const uint8_t *>(spirvMagic.data()), spirvMagic.size(), "some_file.spv"};
    static_cast<MockOclocArgHelper *>(ocloc.argHelper)->inputs.push_back(source);
    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        "some_file.spv",
        "-out_dir",
        "offline_compiler_test",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spirv_input", "-qq"};

    int retVal = ocloc.initialize(argv.size(), argv);
    ASSERT_EQ(0, retVal);
    retVal = ocloc.build();
    EXPECT_EQ(0, retVal);
    EXPECT_FALSE(compilerOutputExists("offline_compiler_test/some_file", "gen"));
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/some_file", "bin"));
    EXPECT_FALSE(compilerOutputExists("offline_compiler_test/some_file", "spv"));
}

TEST_F(OfflineCompilerTests, givenFormatFlagWithKnownFormatPassedThenEnforceSpecifiedFormatAccordingly) {
    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argvEnforcedFormatZebin = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-spv_only",
        "--format",
        "zebin"};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = ocloc.initialize(argvEnforcedFormatZebin.size(), argvEnforcedFormatZebin);
    ASSERT_EQ(0, retVal);
    const auto output = capture.getCapturedStdout();
    EXPECT_TRUE(output.empty());
}

TEST_F(OfflineCompilerTests, givenPatchtokensFormatFlagThenWarningAboutDeprecatedFormatIsProduced) {
    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argvEnforcedFormatPatchtokens = {
        "ocloc",
        "-q",
        "-file",
        clCopybufferFilename.c_str(),
        "-spv_only",
        "--format",
        "patchtokens"};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = ocloc.initialize(argvEnforcedFormatPatchtokens.size(), argvEnforcedFormatPatchtokens);
    const auto output = capture.getCapturedStdout();
    ASSERT_EQ(0, retVal);

    const auto expectedOutput{"WARNING: Ignoring deprecated '--format patchtokens' option.\n"};
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(OfflineCompilerTests, givenFormatFlagWithUnknownFormatPassedThenPrintWarning) {
    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argvUnknownFormatEnforced = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-spv_only",
        "--format",
        "banana"};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = ocloc.initialize(argvUnknownFormatEnforced.size(), argvUnknownFormatEnforced);
    const auto output = capture.getCapturedStdout();
    ASSERT_EQ(0, retVal);

    const auto expectedOutput{"Invalid format passed: banana. Ignoring.\n"};
    EXPECT_EQ(expectedOutput, output);
}

TEST(OfflineCompilerTest, GivenDebugFlagWhenSetStatelessToStatefulBufferOffsetFlagThenStatelessToStatefullOptimizationIsSetCorrectly) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    {
        debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(0);
        mockOfflineCompiler.initHardwareInfo(gEnvironment->devicePrefix.c_str());
        mockOfflineCompiler.setStatelessToStatefulBufferOffsetFlag();
        std::string internalOptions = mockOfflineCompiler.internalOptions;
        size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
        EXPECT_EQ(std::string::npos, found);
    }
    {
        debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);
        mockOfflineCompiler.initHardwareInfo(gEnvironment->devicePrefix.c_str());
        mockOfflineCompiler.setStatelessToStatefulBufferOffsetFlag();
        std::string internalOptions = mockOfflineCompiler.internalOptions;
        size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg.data());
        EXPECT_NE(std::string::npos, found);
    }
}

TEST_F(OfflineCompilerTests, GivenStatefulAddressModeWhenInvalidArgsPAssedThenErrorIsReturned) {
    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argvA = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-stateful_address_mode",
        "wrong"};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = ocloc.initialize(argvA.size(), argvA);
    ASSERT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
    std::string output = capture.getCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");

    std::vector<std::string> argvB = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-stateful_address_mode",
        "bindful",
        "-internal_options",
        "-cl-intel-use-bindless-mode "};

    MockOfflineCompiler ocloc2;

    capture.captureStdout();
    retVal = ocloc2.initialize(argvB.size(), argvB);
    ASSERT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
    output = capture.getCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
}

TEST_F(OfflineCompilerTests, GivenInvalidHeaplessModeOptionWhenOclocArgsAreParsedThenErrorIsReturned) {

    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;
    std::vector<std::string> args = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-heapless_mode",
        "invalid"};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = ocloc.parseCommandLine(args.size(), args);
    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
    std::string output = capture.getCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
}

TEST_F(OfflineCompilerTests, GivenValidHeaplessModeOptionWhenOclocArgsAreParsedThenSuccessIsReturned) {

    for (auto heaplessMode : {"enable", "disable", "default"}) {
        MockOfflineCompiler ocloc;
        ocloc.uniqueHelper->filesMap = filesMap;
        std::vector<std::string> args = {
            "ocloc",
            "-file",
            clCopybufferFilename.c_str(),
            "-device",
            gEnvironment->devicePrefix.c_str(),
            "-heapless_mode",
            heaplessMode};

        StreamCapture capture;
        capture.captureStdout();
        int retVal = ocloc.parseCommandLine(args.size(), args);
        EXPECT_EQ(OCLOC_SUCCESS, retVal);
        std::string output = capture.getCapturedStdout();
        EXPECT_STREQ(output.c_str(), "");
    }
}

TEST_F(OfflineCompilerTests, GivenStatefulAddressModeSetToBindlessWhenBuildThenBindlessModeInternalOptionsAreAdded) {
    MockOfflineCompiler ocloc;
    ocloc.uniqueHelper->filesMap = filesMap;

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-stateful_address_mode",
        "bindless"};

    int retVal = ocloc.initialize(argv.size(), argv);
    ASSERT_EQ(0, retVal);
    retVal = ocloc.build();
    EXPECT_EQ(0, retVal);
    EXPECT_NE(std::string::npos, ocloc.internalOptions.find(NEO::CompilerOptions::bindlessMode.data()));
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

    StreamCapture capture;
    capture.captureStdout();
    helper->printf(printMsg.data());
    std::string capturedStdout = capture.getCapturedStdout();
    EXPECT_TRUE(capturedStdout.empty());

    helper.reset(); // Delete helper. Destructor saves data to output
    EXPECT_EQ(1U, numOutputs);
    EXPECT_EQ(printMsg.length() + 1, lenOutputs[0]);
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

    StreamCapture capture;
    capture.captureStdout();
    helper.printf(printMsg.data());
    std::string capturedStdout = capture.getCapturedStdout();
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

TEST(OclocQuery, WhenQueryingDeviceExtensionsThenExtensionsStringIsReturned) {
    MockOclocArgHelper::FilesMap vfs;
    MockOclocArgHelper argHelper{vfs};
    argHelper.callBaseFileExists = false;
    argHelper.callBaseLoadDataFromFile = false;
    argHelper.callBaseReadBinaryFile = false;
    argHelper.callBaseReadFileToVectorOfStrings = false;
    argHelper.messagePrinter.setSuppressMessages(true);

    std::string query = "CL_DEVICE_EXTENSIONS";
    std::string commonExt = "cl_khr_icd";

    std::vector<std::string> argv = {"ocloc", "query", query};

    int retVal = OfflineCompiler::query(argv.size(), argv, &argHelper);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

    auto log = argHelper.getPrinterRef().getLog().str();
    EXPECT_FALSE(log.empty());
    EXPECT_TRUE(hasSubstr(log, commonExt)) << commonExt << " e/ " << log;

    ASSERT_EQ(1U, vfs.count(query));
    auto file = vfs[query];
    EXPECT_FALSE(file.empty());
    EXPECT_TRUE(hasSubstr(file, commonExt)) << commonExt << " e/ " << file;
}

TEST(OclocQuery, WhenQueryingDeviceExtensionsWithVersionsThenExtensionsStringWithVersionsIsReturned) {
    MockOclocArgHelper::FilesMap vfs;
    MockOclocArgHelper argHelper{vfs};
    argHelper.callBaseFileExists = false;
    argHelper.callBaseLoadDataFromFile = false;
    argHelper.callBaseReadBinaryFile = false;
    argHelper.callBaseReadFileToVectorOfStrings = false;
    argHelper.messagePrinter.setSuppressMessages(true);

    std::string query = "CL_DEVICE_EXTENSIONS_WITH_VERSION";
    std::string commonExt = "cl_khr_icd:1.0.0";
    ocloc_name_version extWithVersion;
    strcpy_s(extWithVersion.name, sizeof(extWithVersion.name), "cl_khr_icd");
    extWithVersion.version = CL_MAKE_VERSION(1, 0, 0);

    std::vector<std::string> argv = {"ocloc", "query", query};

    int retVal = OfflineCompiler::query(argv.size(), argv, &argHelper);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

    auto log = argHelper.getPrinterRef().getLog().str();
    EXPECT_FALSE(log.empty());
    EXPECT_TRUE(hasSubstr(log, commonExt)) << commonExt << " e/ " << log;

    ASSERT_EQ(1U, vfs.count(query));
    auto file = vfs[query];
    EXPECT_NE(0U, file.size());
    EXPECT_EQ(0U, file.size() % sizeof(ocloc_name_version));
    auto it = reinterpret_cast<ocloc_name_version *>(file.data());
    auto end = reinterpret_cast<ocloc_name_version *>(file.data() + file.size());
    decltype(it) found = nullptr;
    for (; it < end; ++it) {
        if ((0 == strcmp(extWithVersion.name, it->name)) && (extWithVersion.version == it->version)) {
            found = it;
        }
    }
    ASSERT_NE(nullptr, found);
}

TEST(OclocQuery, WhenQueryingDeviceProfileThenFullProfileStringIsReturned) {
    MockOclocArgHelper::FilesMap vfs;
    MockOclocArgHelper argHelper{vfs};
    argHelper.callBaseFileExists = false;
    argHelper.callBaseLoadDataFromFile = false;
    argHelper.callBaseReadBinaryFile = false;
    argHelper.callBaseReadFileToVectorOfStrings = false;
    argHelper.messagePrinter.setSuppressMessages(true);

    std::string query = "CL_DEVICE_PROFILE";
    std::string expProfile = "FULL_PROFILE";

    std::vector<std::string> argv = {"ocloc", "query", query};

    int retVal = OfflineCompiler::query(argv.size(), argv, &argHelper);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

    auto log = argHelper.getPrinterRef().getLog().str();
    EXPECT_FALSE(log.empty());
    EXPECT_TRUE(hasSubstr(log, expProfile)) << expProfile << " e/ " << log;

    ASSERT_EQ(1U, vfs.count(query));
    auto file = vfs[query];
    EXPECT_FALSE(file.empty());
    EXPECT_TRUE(hasSubstr(file, expProfile)) << expProfile << " e/ " << file;
}

TEST(OclocQuery, WhenQueryingDeviceOpenCLCAllVersionsThenVersionsStringIsReturned) {
    MockOclocArgHelper::FilesMap vfs;
    MockOclocArgHelper argHelper{vfs};
    argHelper.callBaseFileExists = false;
    argHelper.callBaseLoadDataFromFile = false;
    argHelper.callBaseReadBinaryFile = false;
    argHelper.callBaseReadFileToVectorOfStrings = false;
    argHelper.messagePrinter.setSuppressMessages(true);

    std::string query = "CL_DEVICE_OPENCL_C_ALL_VERSIONS";
    std::string commonVer = "\"OpenCL C\":1.2";
    ocloc_name_version oclcVersion;
    strcpy_s(oclcVersion.name, sizeof(oclcVersion.name), "OpenCL C");
    oclcVersion.version = CL_MAKE_VERSION(1, 2, 0);

    std::vector<std::string> argv = {"ocloc", "query", query};

    int retVal = OfflineCompiler::query(argv.size(), argv, &argHelper);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

    auto log = argHelper.getPrinterRef().getLog().str();
    EXPECT_FALSE(log.empty());
    EXPECT_TRUE(hasSubstr(log, commonVer)) << commonVer << " e/ " << log;

    ASSERT_EQ(1U, vfs.count(query));
    auto file = vfs[query];
    EXPECT_NE(0U, file.size());
    EXPECT_EQ(0U, file.size() % sizeof(ocloc_name_version));
    auto it = reinterpret_cast<ocloc_name_version *>(file.data());
    auto end = reinterpret_cast<ocloc_name_version *>(file.data() + file.size());
    decltype(it) found = nullptr;
    for (; it < end; ++it) {
        if ((0 == strcmp(oclcVersion.name, it->name)) && (oclcVersion.version == it->version)) {
            found = it;
        }
    }
    EXPECT_NE(nullptr, found);
}

TEST(OclocQuery, WhenQueryingDeviceOpenCFeaturesThenFeaturesStringWithVersionsIsReturned) {
    MockOclocArgHelper::FilesMap vfs;
    MockOclocArgHelper argHelper{vfs};
    argHelper.callBaseFileExists = false;
    argHelper.callBaseLoadDataFromFile = false;
    argHelper.callBaseReadBinaryFile = false;
    argHelper.callBaseReadFileToVectorOfStrings = false;
    argHelper.messagePrinter.setSuppressMessages(true);

    std::string query = "CL_DEVICE_OPENCL_C_FEATURES";
    std::string commonFeature = "__opencl_c_int64:3.0.0";
    ocloc_name_version featureWithVersion;
    strcpy_s(featureWithVersion.name, sizeof(featureWithVersion.name), "__opencl_c_int64");
    featureWithVersion.version = CL_MAKE_VERSION(3, 0, 0);

    std::vector<std::string> argv = {"ocloc", "query", query};

    int retVal = OfflineCompiler::query(argv.size(), argv, &argHelper);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

    auto log = argHelper.getPrinterRef().getLog().str();
    EXPECT_FALSE(log.empty());
    EXPECT_TRUE(hasSubstr(log, commonFeature)) << commonFeature << " e/ " << log;

    ASSERT_EQ(1U, vfs.count(query));
    auto file = vfs[query];
    EXPECT_NE(0U, file.size());
    EXPECT_EQ(0U, file.size() % sizeof(ocloc_name_version));
    auto it = reinterpret_cast<ocloc_name_version *>(file.data());
    auto end = reinterpret_cast<ocloc_name_version *>(file.data() + file.size());
    decltype(it) found = nullptr;
    for (; it < end; ++it) {
        if ((0 == strcmp(featureWithVersion.name, it->name)) && (featureWithVersion.version == it->version)) {
            found = it;
        }
    }
    ASSERT_NE(nullptr, found);
}

TEST(OclocOptionsTests, givenInvalidOclocOptionsFileWhenCmdlineIsPrintedThenTheyArePrinted) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    const char kernelSource[] = R"===(
/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void CopyBuffer(__global unsigned int *src, __global unsigned int *dst) {
    int id = (int)get_global_id(0);
    dst[id] = lgamma((float)src[id]);
    dst[id] = src[id];
}
)===";
    Source source{reinterpret_cast<const uint8_t *>(kernelSource), sizeof(kernelSource), "kernel.cl"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(source);

    const char options[] = R"===(
/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

-invalid_ocloc_option
)===";
    Source optionsOcloc{reinterpret_cast<const uint8_t *>(options), sizeof(options), "kernel_ocloc_options.txt"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(optionsOcloc);

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        "kernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    std::string output = capture.getCapturedStdout();
    EXPECT_NE(retVal, OCLOC_SUCCESS);

    EXPECT_TRUE(output.find("Failed with ocloc options from file:\n"
                            "-invalid_ocloc_option") != std::string::npos);
    EXPECT_FALSE(output.find("Building with ocloc options:") != std::string::npos);
}

TEST(OclocOptionsTests, givenInvalidOclocOptionsFileWhenCmdlineIsPrintedThenTheyAreNotPrinted) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    auto kernelSource = R"===(
/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void CopyBuffer(__global unsigned int *src, __global unsigned int *dst) {
    int id = (int)get_global_id(0);
    dst[id] = lgamma((float)src[id]);
    dst[id] = src[id];
}
)===";
    Source source{reinterpret_cast<const uint8_t *>(kernelSource), sizeof(kernelSource), "kernel.cl"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(source);

    const char options[] = R"===(
/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

-invalid_ocloc_option
)===";
    Source optionsOcloc{reinterpret_cast<const uint8_t *>(options), sizeof(options), "kernel_ocloc_options.txt"};
    static_cast<MockOclocArgHelper *>(mockOfflineCompiler->argHelper)->inputs.push_back(optionsOcloc);

    std::vector<std::string> argv = {
        "ocloc",
        "-qq",
        "-file",
        "kernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    StreamCapture capture;
    capture.captureStdout();
    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    std::string output = capture.getCapturedStdout();
    EXPECT_NE(retVal, OCLOC_SUCCESS);

    EXPECT_FALSE(output.find("Failed with ocloc options from file:\n"
                             "-invalid_ocloc_option") != std::string::npos);
    EXPECT_FALSE(output.find("Building with ocloc options:") != std::string::npos);
}

TEST(OclocApiSpecificConfigTests, givenOclocApiConfigThenOnlyOclocPrefixIsAllowed) {
    NEO::ApiSpecificConfig config;
    auto prefixStrings = config.getPrefixStrings();
    ASSERT_EQ(1U, prefixStrings.size());
    EXPECT_STREQ("NEO_OCLOC_", prefixStrings[0]);

    auto prefixStringTypes = config.getPrefixTypes();
    ASSERT_EQ(1U, prefixStringTypes.size());
    EXPECT_EQ(DebugVarPrefix::neoOcloc, prefixStringTypes[0]);
}

TEST(OclocApiSpecificConfigTests, givenOclocThenDebugKeysAreAllowedOnlyInDebug) {
    EXPECT_FALSE(NEO::isDebugKeysReadEnabled());
}

TEST_F(OfflineCompilerTests, GivenFclRedirectionEnvSetToForceIgcWhenInitializingOclocThenIgcIsBeingUsedAsFclFacade) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseIgcAsFcl.set(1);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spv_only"};

    char bin[1] = {7};
    auto oldIgcDebugVars = getIgcDebugVars();
    auto currIgcDebugVars = oldIgcDebugVars;
    currIgcDebugVars.binaryToReturn = bin;
    currIgcDebugVars.binaryToReturnSize = 1;
    currIgcDebugVars.forceBuildFailure = false;
    setIgcDebugVars(currIgcDebugVars);

    auto oldFclDebugVars = getFclDebugVars();
    auto currFclDebugVars = oldFclDebugVars;
    currFclDebugVars.forceBuildFailure = true;
    setFclDebugVars(currFclDebugVars);

    std::unique_ptr<OfflineCompiler> pOfflineCompiler(OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get()));
    ASSERT_NE(nullptr, pOfflineCompiler);
    auto ret = pOfflineCompiler->build();
    EXPECT_EQ(OCLOC_SUCCESS, ret);

    currIgcDebugVars.forceBuildFailure = true;
    setIgcDebugVars(currIgcDebugVars);
    ret = pOfflineCompiler->build();
    EXPECT_NE(OCLOC_SUCCESS, ret);

    setFclDebugVars(oldFclDebugVars);
    setIgcDebugVars(oldIgcDebugVars);
}

TEST_F(OfflineCompilerTests, GivenFclRedirectionEnvSetToForceFclWhenInitializingOclocThenFclIsBeingUsedAsFclFacade) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.UseIgcAsFcl.set(2);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spv_only"};

    char bin[1] = {7};
    auto oldIgcDebugVars = getIgcDebugVars();
    auto currIgcDebugVars = oldIgcDebugVars;
    currIgcDebugVars.forceBuildFailure = true;
    setIgcDebugVars(currIgcDebugVars);

    auto oldFclDebugVars = getFclDebugVars();
    auto currFclDebugVars = oldFclDebugVars;
    currFclDebugVars.binaryToReturn = bin;
    currFclDebugVars.binaryToReturnSize = 1;
    currFclDebugVars.forceBuildFailure = false;
    setFclDebugVars(currFclDebugVars);

    std::unique_ptr<OfflineCompiler> pOfflineCompiler(OfflineCompiler::create(argv.size(), argv, true, retVal, oclocArgHelperWithoutInput.get()));
    ASSERT_NE(nullptr, pOfflineCompiler);
    auto ret = pOfflineCompiler->build();
    EXPECT_EQ(OCLOC_SUCCESS, ret);

    currFclDebugVars.forceBuildFailure = true;
    setFclDebugVars(currFclDebugVars);
    ret = pOfflineCompiler->build();
    EXPECT_NE(OCLOC_SUCCESS, ret);

    setFclDebugVars(oldFclDebugVars);
    setIgcDebugVars(oldIgcDebugVars);
}

TEST_F(OfflineCompilerTests, GivenFclRedirectionDefaultSettingWhenCompilingToIrThenUseCompilerProductHelperDefaults) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(mockOfflineCompiler.compilerProductHelper->useIgcAsFcl(), mockOfflineCompiler.useIgcAsFcl());
    mockOfflineCompiler.compilerProductHelper.reset();
    EXPECT_EQ(false, mockOfflineCompiler.useIgcAsFcl());
}

TEST(OfflineCompilerTest, GivenValidPathWhenCreatingDirectoryThenDirIsCreated) {
    VariableBackup<decltype(IoFunctions::mkdirPtr)> mockCreateDir(&IoFunctions::mkdirPtr, [](const char *path) -> int {
        return 0;
    });

    MockOfflineCompiler mockOfflineCompiler;
    auto ret = mockOfflineCompiler.createDir("/valid/path/dirNames");

    EXPECT_EQ(ret, OCLOC_SUCCESS);
}

TEST(OfflineCompilerTest, GivenNonExistentPathWhenCreatingDirectoryThenReturnInvalidFile) {
    VariableBackup<decltype(IoFunctions::mkdirPtr)> mockCreateDir(&IoFunctions::mkdirPtr, [](const char *path) -> int {
        errno = 0;
        return -1;
    });
    MockOfflineCompiler mockOfflineCompiler{};
    auto ret = mockOfflineCompiler.createDir("/nonexistent/path/dirName");

    EXPECT_EQ(ret, OCLOC_INVALID_FILE);
}

TEST(OfflineCompilerTest, GivenExistingPathWhenCreatingDirectoryThenReturnSuccess) {
    VariableBackup<decltype(IoFunctions::mkdirPtr)> mockCreateDir(&IoFunctions::mkdirPtr, [](const char *path) -> int {
        errno = EEXIST;
        return -1;
    });

    MockOfflineCompiler mockOfflineCompiler;
    auto ret = mockOfflineCompiler.createDir("/existing/path/dirName");
    EXPECT_EQ(ret, OCLOC_SUCCESS);
}

TEST(OfflineCompilerTest, GivenPathWithPermissionDeniedWhenCreatingDirectoryThenReturnInvalidFile) {
    VariableBackup<decltype(IoFunctions::mkdirPtr)> mockCreateDir(&IoFunctions::mkdirPtr, [](const char *path) -> int {
        errno = EACCES;
        return -1;
    });

    MockOfflineCompiler mockOfflineCompiler;
    auto ret = mockOfflineCompiler.createDir("/path/with/permission/denied");

    EXPECT_EQ(ret, OCLOC_INVALID_FILE);
}

TEST_F(OfflineCompilerTests, givenOneApiPvcSendWarWaEnvSetToFalseWhenInitializingThenInternalOptionShouldContainInternalOption) {

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);

    std::unordered_map<std::string, std::string> mockableEnvs = {{"ONEAPI_PVC_SEND_WAR_WA", "0"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        clCopybufferFilename.c_str(),
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->initialize(argv.size(), argv);

    std::string internalOptions = mockOfflineCompiler->internalOptions;
    EXPECT_TRUE(hasSubstr(internalOptions, "-ze-opt-disable-sendwarwa"));
}
TEST_F(OfflineCompilerTests, GivenDebugFlagWhenBuildingFromSourceThenTemporarySourceFileIsCreated) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.uniqueHelper->interceptOutput = true;
    mockOfflineCompiler.uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler.uniqueHelper->callBaseLoadDataFromFile = false;

    if (mockOfflineCompiler.cache == nullptr) {
        mockOfflineCompiler.cache = std::make_unique<CompilerCacheMock>();
    }

    const char kernelSource[] = R"===(
    __kernel void simpleKernel() {
        int gid = get_global_id(0);
    }
    )===";

    std::string inputFile = "kernel.cl";
    mockOfflineCompiler.uniqueHelper->filesMap[inputFile] = kernelSource;

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        inputFile,
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "-g"};

    mockOfflineCompiler.buildReturnValue = OCLOC_SUCCESS;

    int retVal = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

    mockOfflineCompiler.inputCodeType = IGC::CodeType::oclC;
    mockOfflineCompiler.sourceCode = kernelSource;
    mockOfflineCompiler.inputFile = inputFile;

    uint64_t sourceHash = NEO::Hash::hash(kernelSource, strlen(kernelSource));
    std::string tempFileName = "main_" + std::to_string(sourceHash) + ".cl";
    std::filesystem::path expectedTempFilePath = std::filesystem::absolute(tempFileName);

    StreamCapture capture;
    capture.captureStdout();

    mockOfflineCompiler.createTempSourceFileForDebug();

    std::string capturedOutput = capture.getCapturedStdout();
    EXPECT_NE(capturedOutput.find("Temporary source file for debug info created: " + expectedTempFilePath.string()), std::string::npos);

    auto it = NEO::virtualFileList.find(expectedTempFilePath.string());
    ASSERT_NE(NEO::virtualFileList.end(), it);
    EXPECT_EQ(it->second.str(), kernelSource);

    std::string expectedSOption = "-s \"" + expectedTempFilePath.string() + "\"";
    EXPECT_TRUE(CompilerOptions::contains(mockOfflineCompiler.options, expectedSOption));
}
TEST_F(OfflineCompilerTests, GivenSpirvInputAndSpecConstFileWhenFileExistsThenSpecializationConstantsAreLoaded) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;

    const std::array<uint8_t, 4> spirvMagic = {0x03, 0x02, 0x23, 0x07};
    mockOfflineCompiler.uniqueHelper->filesMap["some_file.spv"] = std::string(spirvMagic.begin(), spirvMagic.end());

    mockOfflineCompiler.uniqueHelper->filesMap["constants.txt"] = "0: 42\n1: 100";
    mockOfflineCompiler.uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler.uniqueHelper->callBaseLoadDataFromFile = false;

    std::vector<std::string> argv = {
        "ocloc",
        "-file", "some_file.spv",
        "-spirv_input",
        "-device", gEnvironment->devicePrefix.c_str(),
        "-spec_const", "constants.txt",
        "-v"};

    StreamCapture capture;
    capture.captureStdout();

    mockOfflineCompiler.initialize(argv.size(), argv);
    std::string output = capture.getCapturedStdout();
    EXPECT_NE(output.find("Loaded 2 specialization constants from: constants.txt"), std::string::npos);

    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.mockIgcFacade->igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxTagOCL>(mockIgcOclDeviceCtx);

    int retVal = mockOfflineCompiler.build();
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    ASSERT_EQ(2u, mockOfflineCompiler.specConstants.size());
    EXPECT_EQ(42u, mockOfflineCompiler.specConstants[0]);
    EXPECT_EQ(100u, mockOfflineCompiler.specConstants[1]);
}

TEST_F(OfflineCompilerTests, GivenSpirvInputAndSpecConstFileWhenFileIsMissingThenInvalidFileErrorIsReturned) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;
    const std::array<uint8_t, 4> spirvMagic = {0x03, 0x02, 0x23, 0x07};
    mockOfflineCompiler.uniqueHelper->filesMap["some_file.spv"] = std::string(spirvMagic.begin(), spirvMagic.end());
    mockOfflineCompiler.uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler.uniqueHelper->callBaseLoadDataFromFile = false;

    std::vector<std::string> argv = {
        "ocloc",
        "-file", "some_file.spv",
        "-spirv_input",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spec_const", "constants.txt",
        "-v"};

    StreamCapture capture;
    capture.captureStdout();

    int initResult = mockOfflineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(OCLOC_INVALID_FILE, initResult);

    std::string output = capture.getCapturedStdout();
    EXPECT_NE(output.find("Error: Spec constants file not found: constants.txt"), std::string::npos);
}
TEST_F(OfflineCompilerTests, GivenSpirvInputAndSpecConstFileAndNoVerboseModeWhenFileExistsThenSpecializationConstantsAreLoadedAndErrorMessageIsSuppressed) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.uniqueHelper->filesMap = filesMap;

    const std::array<uint8_t, 4> spirvMagic = {0x03, 0x02, 0x23, 0x07};
    mockOfflineCompiler.uniqueHelper->filesMap["some_file.spv"] = std::string(spirvMagic.begin(), spirvMagic.end());

    mockOfflineCompiler.uniqueHelper->filesMap["constants.txt"] = "0: 42\n1: 100";
    mockOfflineCompiler.uniqueHelper->callBaseFileExists = false;
    mockOfflineCompiler.uniqueHelper->callBaseLoadDataFromFile = false;

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "some_file.spv",
        "-spirv_input",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spec_const",
        "constants.txt",
    };

    StreamCapture capture;
    capture.captureStdout();

    mockOfflineCompiler.initialize(argv.size(), argv);
    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(output.find("Loaded 2 specialization constants from: constants.txt"), std::string::npos);

    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.mockIgcFacade->igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxTagOCL>(mockIgcOclDeviceCtx);

    int retVal = mockOfflineCompiler.build();
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    ASSERT_EQ(2u, mockOfflineCompiler.specConstants.size());
    EXPECT_EQ(42u, mockOfflineCompiler.specConstants[0]);
    EXPECT_EQ(100u, mockOfflineCompiler.specConstants[1]);
}
TEST_F(OfflineCompilerTests, GivenVariousLinesInSpecConstFileWhenParsingThenFileIsValidatedWithProperErrorCodes) {
    struct TestCase {
        std::string fileContent;
        int expectedReturnCode;
        std::map<uint32_t, uint64_t> expectedConstants;
    };

    std::vector<TestCase> cases = {
        {"0: 123", OCLOC_SUCCESS, {{0, 123}}},
        {"1: 90", OCLOC_SUCCESS, {{1, 90}}},
        {"abc: 2", OCLOC_INVALID_COMMAND_LINE, {}},
        {"1; 90", OCLOC_INVALID_COMMAND_LINE, {}},
        {"90", OCLOC_INVALID_COMMAND_LINE, {}},
        {"1: abc", OCLOC_INVALID_COMMAND_LINE, {}},
        {"abc: abc", OCLOC_INVALID_COMMAND_LINE, {}},
        {"   ", OCLOC_INVALID_COMMAND_LINE, {}},
        {"garbage", OCLOC_INVALID_COMMAND_LINE, {}},
        {"1234", OCLOC_INVALID_COMMAND_LINE, {}},
        {"123.0", OCLOC_INVALID_COMMAND_LINE, {}},
        {"123.0: 1", OCLOC_INVALID_COMMAND_LINE, {}},

        {"0: 123\n1: 90", OCLOC_SUCCESS, {{0, 123}, {1, 90}}},
        {"0: 123\n\n2: 456", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 123\n   \n2: 456", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 123\n1: abc\n2: 456", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 123\n90\n2: 456", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 123\n1; 90\n2: 456", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 123\n2: 456\n", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 123\n2: 456\n   ", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 123\n1: 456\n0: 789", OCLOC_INVALID_COMMAND_LINE, {}},
        {"-1: 123", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: -1", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 123\n1: 123abc\n2: 456", OCLOC_INVALID_COMMAND_LINE, {}},
        {": 123", OCLOC_INVALID_COMMAND_LINE, {}},
        {"123:", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0:123", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: ", OCLOC_INVALID_COMMAND_LINE, {}},
        {"0: 184467440737095516160", OCLOC_INVALID_COMMAND_LINE, {}},
        {"1: 10\n2: 20", OCLOC_INVALID_COMMAND_LINE, {}},
        {"", OCLOC_INVALID_FILE, {}}};

    for (const auto &testCase : cases) {

        MockOfflineCompiler mockOfflineCompiler;
        mockOfflineCompiler.uniqueHelper->filesMap["constants.txt"] = testCase.fileContent;
        mockOfflineCompiler.uniqueHelper->callBaseFileExists = false;
        mockOfflineCompiler.uniqueHelper->callBaseLoadDataFromFile = false;

        StreamCapture capture;
        capture.captureStdout();

        int result = mockOfflineCompiler.loadSpecializationConstants("constants.txt");
        capture.getCapturedStdout();

        EXPECT_EQ(testCase.expectedReturnCode, result);

        if (testCase.expectedReturnCode == OCLOC_SUCCESS) {
            EXPECT_EQ(testCase.expectedConstants.size(), mockOfflineCompiler.specConstants.size());

            for (const auto &[id, value] : testCase.expectedConstants) {
                ASSERT_NE(mockOfflineCompiler.specConstants.end(), mockOfflineCompiler.specConstants.find(id));
                EXPECT_EQ(value, mockOfflineCompiler.specConstants.at(id));
            }
        } else {
            EXPECT_TRUE(mockOfflineCompiler.specConstants.empty());
        }
    }
}
} // namespace NEO
