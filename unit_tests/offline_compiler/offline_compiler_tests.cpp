/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "environment.h"
#include "mock/mock_offline_compiler.h"
#include "offline_compiler_tests.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "gmock/gmock.h"

#include <algorithm>

#define ARRAY_COUNT(x) (sizeof(x) / sizeof(x[0]))

extern Environment *gEnvironment;

namespace OCLRT {

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

TEST_F(OfflineCompilerTests, GoodArgTest) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, TestExtensions) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);
    mockOfflineCompiler->parseCommandLine(ARRAY_COUNT(argv), argv);
    std::string internalOptions = mockOfflineCompiler->getInternalOptions();
    EXPECT_THAT(internalOptions, ::testing::HasSubstr(std::string("cl_khr_3d_image_writes")));
}

TEST_F(OfflineCompilerTests, GoodBuildTest) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    testing::internal::CaptureStdout();
    retVal = pOfflineCompiler->build();
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "bc"));
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "gen"));
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "bin"));

    std::string buildLog = pOfflineCompiler->getBuildLog();
    EXPECT_STREQ(buildLog.c_str(), "");

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GoodBuildTestWithLlvmText) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-llvm_text"};

    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "ll"));
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "gen"));
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "bin"));

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GoodParseBinToCharArray) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);
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
                                              "#include \"runtime/built_ins/registry/built_ins_registry.h\"\n\n"
                                              "namespace OCLRT {\n"
                                              "static RegisterEmbeddedResource registerSchedulerBin(\n"
                                              "    createBuiltinResourceName(\n"
                                              "        EBuiltInOps::Scheduler,\n"
                                              "        BuiltinCode::getExtension(BuiltinCode::ECodeType::Binary), \"" +
                         familyNameWithType + "\", 0)\n"
                                              "        .c_str(),\n"
                                              "    (const char *)SchedulerBinary_" +
                         familyNameWithType + ",\n"
                                              "    SchedulerBinarySize_" +
                         familyNameWithType + ");\n"
                                              "}\n";
    EXPECT_EQ(retArray, target);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GoodBuildTestWithCppFile) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-cpp_file"};

    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "cpp"));
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "bc"));
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "gen"));
    EXPECT_EQ(true, compilerOutputExists("copybuffer", "bin"));

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, GoodBuildTestWithOutputDir) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-out_dir",
        "offline_compiler_test"};

    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);

    EXPECT_NE(nullptr, pOfflineCompiler);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = pOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(true, compilerOutputExists("offline_compiler_test/copybuffer", "bc"));
    EXPECT_EQ(true, compilerOutputExists("offline_compiler_test/copybuffer", "gen"));
    EXPECT_EQ(true, compilerOutputExists("offline_compiler_test/copybuffer", "bin"));

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, PrintUsage) {
    const char *argv[] = {
        "cloc",
        "-?"};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_STRNE("", output.c_str());
    EXPECT_EQ(PRINT_USAGE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, NaughtyArgTest_File) {
    DebugManager.flags.PrintDebugMessages.set(true);
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(INVALID_FILE, retVal);
    DebugManager.flags.PrintDebugMessages.set(false);
    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, NaughtyArgTest_Flag) {
    const char *argv[] = {
        "cloc",
        "-n",
        "test_files/ImANaughtyFile.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, NaughtyArgTest_NumArgs) {
    const char *argv_a[] = {
        "cloc",
        "-file",
    };

    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv_a), argv_a, retVal);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");

    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;

    const char *argv_b[] = {
        "cloc",
        "-file",
        "test_files/ImANaughtyFile.cl",
        "-device"};
    testing::internal::CaptureStdout();
    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv_b), argv_b, retVal);
    output = testing::internal::GetCapturedStdout();
    EXPECT_STRNE(output.c_str(), "");
    EXPECT_EQ(nullptr, pOfflineCompiler);
    EXPECT_EQ(INVALID_COMMAND_LINE, retVal);

    delete pOfflineCompiler;
}

TEST_F(OfflineCompilerTests, NaughtyKernelTest) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/shouldfail.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    pOfflineCompiler = OfflineCompiler::create(ARRAY_COUNT(argv), argv, retVal);

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
    const char *argv[] = {
        "cloc",
        "-cl-intel-greater-than-4GB-buffer-required"};

    MockOfflineCompiler *mockOfflineCompiler = new MockOfflineCompiler();
    ASSERT_NE(nullptr, mockOfflineCompiler);

    testing::internal::CaptureStdout();
    mockOfflineCompiler->parseCommandLine(ARRAY_COUNT(argv), argv);
    std::string output = testing::internal::GetCapturedStdout();

    std::string internalOptions = mockOfflineCompiler->getInternalOptions();
    size_t found = internalOptions.find(argv[1]);
    EXPECT_NE(std::string::npos, found);

    delete mockOfflineCompiler;
}

TEST(OfflineCompilerTest, givenStatelessToStatefullOptimizationEnabledWhenDebugSettingsAreParsedThenOptimizationStringIsPresent) {
    DebugManagerStateRestore stateRestore;
    MockOfflineCompiler mockOfflineCompiler;
    DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(1);

    mockOfflineCompiler.parseDebugSettings();

    std::string internalOptions = mockOfflineCompiler.getInternalOptions();
    size_t found = internalOptions.find("-cl-intel-has-buffer-offset-arg");
    EXPECT_NE(std::string::npos, found);
}

TEST(OfflineCompilerTest, getStringWithinDelimiters) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    void *ptrSrc = nullptr;
    size_t srcSize = loadDataFromFile("test_files/copy_buffer_to_buffer.igdrcl_built_in", ptrSrc);

    const std::string src = (const char *)ptrSrc;
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

    delete[] reinterpret_cast<char *>(ptrSrc);
}

TEST(OfflineCompilerTest, convertToPascalCase) {
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

    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(ARRAY_COUNT(argv), argv);
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

    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/emptykernel.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(ARRAY_COUNT(argv), argv);
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
    binHeader.Device = platformDevices[0]->pPlatform->eRenderCoreFamily;
    binHeader.GPUPointerSizeInBytes = 8;
    binHeader.NumberOfKernels = 0;
    binHeader.SteppingId = 0;
    binHeader.PatchListSize = 0;
    size_t binSize = sizeof(iOpenCL::SProgramBinaryHeader);

    mockOfflineCompiler->storeGenBinary(&binHeader, binSize);

    EXPECT_EQ(nullptr, mockOfflineCompiler->getElfBinary());
    EXPECT_EQ(0u, mockOfflineCompiler->getElfBinarySize());

    retVal = mockOfflineCompiler->generateElfBinary();
    EXPECT_TRUE(retVal);

    EXPECT_NE(nullptr, mockOfflineCompiler->getElfBinary());
    EXPECT_NE(0u, mockOfflineCompiler->getElfBinarySize());
}

TEST(OfflineCompilerTest, givenLlvmInputOptionPassedWhenCmdLineParsedThenInputFileLlvmIsSetTrue) {
    const char *argv[] = {
        "cloc",
        "-llvm_input"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    testing::internal::CaptureStdout();
    mockOfflineCompiler->parseCommandLine(ARRAY_COUNT(argv), argv);
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

TEST(OfflineCompilerTest, givenLlvmInputFileAndLlvmInputFlagWhenBuildSourceCodeIsCalledThenGenBinaryIsProduced) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    auto retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);

    std::string sipKernelFileName = "test_files/sip_dummy_kernel";

    if (sizeof(uintptr_t) == 8) {
        sipKernelFileName += "_64.ll";
    } else {
        sipKernelFileName += "_32.ll";
    }

    const char *argv[] = {
        "cloc",
        "-file",
        sipKernelFileName.c_str(),
        "-llvm_input",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    retVal = mockOfflineCompiler->initialize(ARRAY_COUNT(argv), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(nullptr, mockOfflineCompiler->getGenBinary());
    EXPECT_EQ(0u, mockOfflineCompiler->getGenBinarySize());

    retVal = mockOfflineCompiler->buildSourceCode();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, mockOfflineCompiler->getGenBinary());
    EXPECT_NE(0u, mockOfflineCompiler->getGenBinarySize());
}

TEST(OfflineCompilerTest, givenOutputFileOptionWhenSourceIsCompiledThenOutputFileHasCorrectName) {
    const char *argv[] = {
        "cloc",
        "-file",
        "test_files/copybuffer.cl",
        "-output",
        "myOutputFileName",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    int retVal = mockOfflineCompiler->initialize(ARRAY_COUNT(argv), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bc"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));

    retVal = mockOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bc"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "gen"));

    compilerOutputRemove("myOutputFileName", "bc");
    compilerOutputRemove("myOutputFileName", "bin");
    compilerOutputRemove("myOutputFileName", "gen");
}

TEST(OfflineCompilerTest, givenDebugDataAvailableWhenSourceIsBuiltThenDebugDataFileIsCreated) {
    const char *argv[] = {
        "cloc",
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

    OCLRT::setIgcDebugVars(igcDebugVars);

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    int retVal = mockOfflineCompiler->initialize(ARRAY_COUNT(argv), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bc"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "gen"));
    EXPECT_FALSE(compilerOutputExists("myOutputFileName", "dbg"));

    retVal = mockOfflineCompiler->build();
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bc"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "bin"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "gen"));
    EXPECT_TRUE(compilerOutputExists("myOutputFileName", "dbg"));

    compilerOutputRemove("myOutputFileName", "bc");
    compilerOutputRemove("myOutputFileName", "bin");
    compilerOutputRemove("myOutputFileName", "gen");
    compilerOutputRemove("myOutputFileName", "dbg");

    OCLRT::setIgcDebugVars(gEnvironment->igcDebugVars);
}

TEST(OfflineCompilerTest, givenInternalOptionsWhenCmdLineParsedThenOptionsAreAppendedToInternalOptionsString) {
    const char *argv[] = {
        "cloc",
        "-internal_options",
        "myInternalOptions"};

    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    testing::internal::CaptureStdout();
    mockOfflineCompiler->parseCommandLine(ARRAY_COUNT(argv), argv);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(0u, output.size());

    std::string internalOptions = mockOfflineCompiler->getInternalOptions();
    EXPECT_THAT(internalOptions, ::testing::HasSubstr(std::string("myInternalOptions")));
}

TEST(OfflineCompilerTest, givenInputOtpionsAndInternalOptionsFilesWhenOfflineCompilerIsInitializedThenCorrectOptionsAreSetAndRemainAfterBuild) {
    auto mockOfflineCompiler = std::unique_ptr<MockOfflineCompiler>(new MockOfflineCompiler());
    ASSERT_NE(nullptr, mockOfflineCompiler);

    ASSERT_TRUE(fileExists("test_files/shouldfail_options.txt"));
    ASSERT_TRUE(fileExists("test_files/shouldfail_internal_options.txt"));

    const char *argv[] = {
        "cloc",
        "-q",
        "-file",
        "test_files/shouldfail.cl",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    int retVal = mockOfflineCompiler->initialize(ARRAY_COUNT(argv), argv);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &options = mockOfflineCompiler->getOptions();
    auto &internalOptions = mockOfflineCompiler->getInternalOptions();
    EXPECT_STREQ(options.c_str(), "-shouldfailOptions");
    EXPECT_STREQ(internalOptions.c_str(), "-shouldfailInternalOptions");

    mockOfflineCompiler->build();

    EXPECT_STREQ(options.c_str(), "-shouldfailOptions");
    EXPECT_STREQ(internalOptions.c_str(), "-shouldfailInternalOptions");
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

} // namespace OCLRT
