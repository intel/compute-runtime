/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler_tests.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_cmds.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "opencl/test/unit_test/mocks/mock_compilers.h"

#include "compiler_options.h"
#include "environment.h"
#include "gmock/gmock.h"
#include "mock/mock_offline_compiler.h"

#include <algorithm>
#include <fstream>

extern Environment *gEnvironment;

namespace NEO {

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

TEST_F(MultiCommandTests, MultiCommandSuccessfulBuildTest) {
    nameOfFileWithArgs = "test_files/ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "-multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal));

    EXPECT_NE(nullptr, pMultiCommand);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteFileWithArgs();
}
TEST_F(MultiCommandTests, MultiCommandSuccessfulBuildWithOutputFileTest) {
    nameOfFileWithArgs = "test_files/ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "-multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal));

    EXPECT_NE(nullptr, pMultiCommand);
    EXPECT_EQ(CL_SUCCESS, retVal);

    for (int i = 0; i < numOfBuild; i++) {
        std::string outFileName = pMultiCommand->outDirForBuilds + "/build_no_" + std::to_string(i + 1);
        EXPECT_TRUE(compilerOutputExists(outFileName, "bc") || compilerOutputExists(outFileName, "spv"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "gen"));
        EXPECT_TRUE(compilerOutputExists(outFileName, "bin"));
    }

    for (OfflineCompiler *pSingle : pMultiCommand->singleBuilds) {
        std::string buildLog = pSingle->getBuildLog();
        EXPECT_STREQ(buildLog.c_str(), "");
    }

    deleteFileWithArgs();
}
TEST_F(MultiCommandTests, GoodMultiBuildTestWithspecifiedOutputDir) {
    nameOfFileWithArgs = "test_files/ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "-multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-out_dir",
        "offline_compiler_test"};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    pMultiCommand = MultiCommand::create(argv, retVal);

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
TEST_F(MultiCommandTests, LackOfTxtFileWithArgsMultiTest) {
    nameOfFileWithArgs = "test_files/ImANotExistedComandFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "-multi",
        "test_files/ImANaughtyFile.txt",
        "-q",
    };

    testing::internal::CaptureStdout();
    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal));
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pMultiCommand);
    EXPECT_EQ(INVALID_COMMAND_LINE, retVal);
    DebugManager.flags.PrintDebugMessages.set(false);
}
TEST_F(MultiCommandTests, LackOfClFilePointedInTxtFileMultiTest) {
    nameOfFileWithArgs = "test_files/ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "-multi",
        nameOfFileWithArgs.c_str(),
        "-q",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        "test_files/ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);
    testing::internal::CaptureStdout();
    auto pMultiCommand = std::unique_ptr<MultiCommand>(MultiCommand::create(argv, retVal));
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pMultiCommand);
    EXPECT_EQ(INVALID_FILE, retVal);
    DebugManager.flags.PrintDebugMessages.set(false);

    deleteFileWithArgs();
}
TEST_F(MultiCommandTests, GoodMultiBuildTestWithOutputFileListFlag) {
    nameOfFileWithArgs = "test_files/ImAMulitiComandMinimalGoodFile.txt";
    std::vector<std::string> argv = {
        "ocloc",
        "-multi",
        nameOfFileWithArgs.c_str(),
        "-q",
        "-output_file_list",
        "outFileList.txt",
    };

    std::vector<std::string> singleArgs = {
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int numOfBuild = 4;
    createFileWithArgs(singleArgs, numOfBuild);

    pMultiCommand = MultiCommand::create(argv, retVal);

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
TEST_F(OfflineCompilerTests, GoodArgTest) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, TestExtensions) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string internalOptions = mockOfflineCompiler->getInternalOptions();
    EXPECT_THAT(internalOptions, ::testing::HasSubstr(std::string("cl_khr_3d_image_writes")));
}
TEST_F(OfflineCompilerTests, GoodBuildTest) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);

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

TEST_F(OfflineCompilerTests, GoodBuildTestWithLlvmText) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-llvm_text"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("copybuffer", "ll"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "gen"));
    EXPECT_TRUE(compilerOutputExists("copybuffer", "bin"));

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, WhenFclNotNeededDontLoadIt) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spirv_input"};

    MockOfflineCompiler offlineCompiler;
    auto ret = offlineCompiler.initialize(argv.size(), argv);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(nullptr, offlineCompiler.fclDeviceCtx);
    EXPECT_NE(nullptr, offlineCompiler.igcDeviceCtx);
}

TEST_F(OfflineCompilerTests, GoodParseBinToCharArray) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);
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
                                              "#include \"opencl/source/built_ins/registry/built_ins_registry.h\"\n\n"
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
TEST_F(OfflineCompilerTests, GoodBuildTestWithCppFile) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-cpp_file"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);

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
TEST_F(OfflineCompilerTests, GoodBuildTestWithOutputDir) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-out_dir",
        "offline_compiler_test"};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/copybuffer", "bc") || compilerOutputExists("offline_compiler_test/copybuffer", "spv"));
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/copybuffer", "gen"));
    EXPECT_TRUE(compilerOutputExists("offline_compiler_test/copybuffer", "bin"));

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, PrintUsage) {
    std::vector<std::string> argv = {
        "ocloc",
        "--help"};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STRNE("", output.c_str());
    EXPECT_EQ(PRINT_USAGE, retVal);

    delete pOfflineCompiler;
}
TEST_F(OfflineCompilerTests, NaughtyArgTest_File) {
    DebugManager.flags.PrintDebugMessages.set(true);
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(INVALID_FILE, retVal);
    DebugManager.flags.PrintDebugMessages.set(false);
    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, NaughtyArgTest_Flag) {
    std::vector<std::string> argv = {
        "ocloc",
        "-n",
        "test_files/ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, NaughtyArgTest_NumArgs) {
    std::vector<std::string> argvA = {
        "ocloc",
        "-file",
    };

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argvA.size(), argvA, true, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");

    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;

    std::vector<std::string> argvB = {
        "ocloc",
        "-file",
        "test_files/ImANaughtyFile.cl",
        "-device"};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argvB.size(), argvB, true, retVal);
    output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GivenNonexistantDeviceWhenCompilingThenExitWithErrorMsg) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        "foobar"};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "Error: Cannot get HW Info for device foobar.\n");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_INVALID_DEVICE, retVal);
}

TEST_F(OfflineCompilerTests, NaughtyKernelTest) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/shouldfail.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(argv.size(), argv, true, retVal);

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

TEST(OfflineCompilerTest, parseCmdLine) {
    std::vector<std::string> argv = {
        "ocloc",
        NEO::CompilerOptions::greaterThan4gbBuffersRequired.data()};

    MockOfflineCompiler *mockOfflineCompiler = new MockOfflineCompiler();
    ASSERT_NE(nullptr, mockOfflineCompiler);

    testing::internal::CaptureStdout();
    mockOfflineCompiler->parseCommandLine(argv.size(), argv);
    std::string output = testing::internal::GetCapturedStdout();

    std::string internalOptions = mockOfflineCompiler->getInternalOptions();
    size_t found = internalOptions.find(argv.begin()[1]);
    EXPECT_NE(std::string::npos, found);

    delete mockOfflineCompiler;
}

TEST(OfflineCompilerTest, givenStatelessToStatefullOptimizationEnabledWhenDebugSettingsAreParsedThenOptimizationStringIsPresent) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.getInternalOptions();
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg);
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, givenStatelessToStatefullOptimizationEnabledWhenDebugSettingsAreParsedThenOptimizationStringIsSetToDefault) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(-1);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.getInternalOptions();
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg);
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, givenStatelessToStatefullOptimizationDisableddWhenDeviceNameIsSetToBDW) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "bdw";

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.getInternalOptions();
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg);
    EXPECT_EQ(std::string::npos, found);
}

TEST(OfflineCompilerTest, givenStatelessToStatefullOptimizationEnabledWhenDeviceNameIsSetToSKL) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "skl";

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.getInternalOptions();
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg);
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, givenStatelessToStatefullOptimizationDisabledWhenDeviceNameIsSetToSKLAndDebugSettingsAreDisabled) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.deviceName = "skl";
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(0);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.getInternalOptions();
    size_t found = internalOptions.find(NEO::CompilerOptions::hasBufferOffsetArg);
    EXPECT_EQ(std::string::npos, found);
}

TEST(OfflineCompilerTest, getStringWithinDelimiters) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    size_t srcSize = 0;
    auto ptrSrc = loadDataFromFile("test_files/copy_buffer_to_buffer.builtin_kernel", srcSize);

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

TEST(OfflineCompilerTest, convertToPascalCase) {
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

TEST(OfflineCompilerTest, getHardwareInfo) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    EXPECT_EQ(CL_INVALID_DEVICE, mockOfflineCompiler->getHardwareInfo("invalid"));
    EXPECT_EQ(CL_SUCCESS, mockOfflineCompiler->getHardwareInfo(gEnvironment->devicePrefix.c_str()));
}

TEST(OfflineCompilerTest, storeBinary) {
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

TEST(OfflineCompilerTest, updateBuildLog) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    std::string ErrorString = "Error: undefined variable";
    mockOfflineCompiler->updateBuildLog(ErrorString.c_str(), ErrorString.length());
    EXPECT_EQ(0, ErrorString.compare(mockOfflineCompiler->getBuildLog()));

    std::string FinalString = "Build failure";
    mockOfflineCompiler->updateBuildLog(FinalString.c_str(), FinalString.length());
    EXPECT_EQ(0, (ErrorString + "\n" + FinalString).compare(mockOfflineCompiler->getBuildLog().c_str()));
}

TEST(OfflineCompilerTest, buildSourceCode) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, mockOfflineCompiler->getGenBinary());
    EXPECT_EQ(0u, mockOfflineCompiler->getGenBinarySize());

    retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, mockOfflineCompiler->getGenBinary());
    EXPECT_NE(0u, mockOfflineCompiler->getGenBinarySize());
}

TEST(OfflineCompilerTest, GivenKernelWhenNoCharAfterKernelSourceThenBuildWithSuccess) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/emptykernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(OfflineCompilerTest, generateElfBinary) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->generateElfBinary();
    EXPECT_FALSE(retVal);

    iOpenCL::SProgramBinaryHeader binHeader;
    memset(&binHeader, 0, sizeof(binHeader));
    binHeader.Magic = iOpenCL::MAGIC_CL;
    binHeader.Version = iOpenCL::CURRENT_ICBE_VERSION - 3;
    binHeader.Device = DEFAULT_PLATFORM::hwInfo.platform.eRenderCoreFamily;
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

TEST(OfflineCompilerTest, givenIntermediatedRepresentationInputWhenBuildSourceCodeIsCalledThenProperTranslationContextIsUsed) {
    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/emptykernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto retVal = mockOfflineCompiler.initialize(argv.size(), argv);
    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxLatest>(mockIgcOclDeviceCtx);
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
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(1U, mockIgcOclDeviceCtx->requestedTranslationCtxs.size());
    expectedTranslation = {IGC::CodeType::llvmBc, IGC::CodeType::oclGenBin};
    ASSERT_EQ(expectedTranslation, mockIgcOclDeviceCtx->requestedTranslationCtxs[0]);
}

TEST(OfflineCompilerTest, givenBinaryInputThenDontTruncateSourceAtFirstZero) {
    std::vector<std::string> argvLlvm = {"ocloc", "-llvm_input", "-file", "test_files/binary_with_zeroes",
                                         "-device", gEnvironment->devicePrefix.c_str()};
    auto mockOfflineCompiler = std::make_unique<MockOfflineCompiler>();
    mockOfflineCompiler->initialize(argvLlvm.size(), argvLlvm);
    EXPECT_LT(0U, mockOfflineCompiler->sourceCode.size());

    std::vector<std::string> argvSpirV = {"ocloc", "-spirv_input", "-file", "test_files/binary_with_zeroes",
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
        "test_files/emptykernel.cl",
        "-spirv_input",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-options",
        "test_options_passed"};

    auto retVal = mockOfflineCompiler.initialize(argv.size(), argv);
    auto mockIgcOclDeviceCtx = new NEO::MockIgcOclDeviceCtx();
    mockOfflineCompiler.igcDeviceCtx = CIF::RAII::Pack<IGC::IgcOclDeviceCtxLatest>(mockIgcOclDeviceCtx);
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
        "test_files/copybuffer.cl",
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

    compilerOutputRemove("myOutputFileName", "bc");
    compilerOutputRemove("myOutputFileName", "spv");
    compilerOutputRemove("myOutputFileName", "bin");
    compilerOutputRemove("myOutputFileName", "gen");
}

TEST(OfflineCompilerTest, givenDebugDataAvailableWhenSourceIsBuiltThenDebugDataFileIsCreated) {
    std::vector<std::string> argv = {
        "ocloc",
        "-file",
        "test_files/copybuffer.cl",
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

    compilerOutputRemove("myOutputFileName", "bc");
    compilerOutputRemove("myOutputFileName", "spv");
    compilerOutputRemove("myOutputFileName", "bin");
    compilerOutputRemove("myOutputFileName", "gen");
    compilerOutputRemove("myOutputFileName", "dbg");

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

    std::string internalOptions = mockOfflineCompiler->getInternalOptions();
    EXPECT_THAT(internalOptions, ::testing::HasSubstr(std::string("myInternalOptions")));
}

TEST(OfflineCompilerTest, givenInputOptionsAndInternalOptionsFilesWhenOfflineCompilerIsInitializedThenCorrectOptionsAreSetAndRemainAfterBuild) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    ASSERT_TRUE(fileExists("test_files/shouldfail_options.txt"));
    ASSERT_TRUE(fileExists("test_files/shouldfail_internal_options.txt"));

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        "test_files/shouldfail.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &options = mockOfflineCompiler->getOptions();
    auto &internalOptions = mockOfflineCompiler->getInternalOptions();
    EXPECT_STREQ(options.c_str(), "-shouldfailOptions");
    EXPECT_TRUE(internalOptions.find("-shouldfailInternalOptions") != std::string::npos);

    mockOfflineCompiler->build();

    EXPECT_STREQ(options.c_str(), "-shouldfailOptions");
    EXPECT_TRUE(internalOptions.find("-shouldfailInternalOptions") != std::string::npos);
}

TEST(OfflineCompilerTest, givenInputOptionsAndOclockOptionsFileWithForceStosOptWhenOfflineCompilerIsInitializedThenCompilerOptionGreaterThan4gbBuffersRequiredIsNotApplied) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    ASSERT_TRUE(fileExists("test_files/stateful_copy_buffer_ocloc_options.txt"));

    std::vector<std::string> argv = {
        "ocloc",
        "-q",
        "-file",
        "test_files/stateful_copy_buffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(argv.size(), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    mockOfflineCompiler->build();

    auto &internalOptions = mockOfflineCompiler->getInternalOptions();
    size_t found = internalOptions.find(NEO::CompilerOptions::greaterThan4gbBuffersRequired);
    EXPECT_EQ(std::string::npos, found);
}

TEST(OfflineCompilerTest, givenNonExistingFilenameWhenUsedToReadOptionsThenReadOptionsFromFileReturnsFalse) {
    std::string options;
    std::string file("non_existing_file");
    ASSERT_FALSE(fileExists(file.c_str()));

    bool result = OfflineCompiler::readOptionsFromFile(options, file);

    EXPECT_FALSE(result);
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

} // namespace NEO
