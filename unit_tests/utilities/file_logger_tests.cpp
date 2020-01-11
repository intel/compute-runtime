/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/utilities/file_logger_tests.h"

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "core/unit_tests/utilities/base_object_utils.h"
#include "runtime/utilities/logger.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_program.h"

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>

using namespace std;
using namespace NEO;

TEST(FileLogger, WithDebugFunctionality) {
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(std::string(""), flags);

    EXPECT_TRUE(fileLogger.enabled());
}

TEST(FileLogger, GivenFileLoggerWhenSettingFileNameThenCorrectFilenameIsSet) {
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(std::string(""), flags);
    fileLogger.setLogFileName("new_filename");
    EXPECT_STREQ("new_filename", fileLogger.getLogFileName());
}

TEST(FileLogger, WithDebugFunctionalityCreatesAndDumpsToLogFile) {
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(std::string("test.log"), flags);

    fileLogger.logApiCall("searchString", true, 0);
    fileLogger.logApiCall("searchString2", false, 0);
    fileLogger.logInputs("searchString3", "any");
    fileLogger.logInputs("searchString3", "any", "and more");
    fileLogger.log(false, "searchString4");

    // Log file not created
    EXPECT_TRUE(fileLogger.wasFileCreated(fileLogger.getLogFileName()));

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find("searchString") != std::string::npos);
        EXPECT_TRUE(str.find("searchString2") != std::string::npos);
        EXPECT_TRUE(str.find("searchString3") != std::string::npos);
        EXPECT_FALSE(str.find("searchString4") != std::string::npos);
    }

    fileLogger.log(true, "searchString4");

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find("searchString4") != std::string::npos);
    }
}

TEST(FileLogger, WithoutDebugFunctionalityDoesNotCreateLogFile) {
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyDisabledFileLogger fileLogger(std::string("  "), flags);

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    fileLogger.logApiCall("searchString", true, 0);
    fileLogger.logApiCall("searchString2", false, 0);
    fileLogger.logInputs("searchString3", "any");
    fileLogger.logInputs("searchString3", "any", "and more");
    fileLogger.log(false, "searchString4");

    EXPECT_FALSE(fileLogger.wasFileCreated(fileLogger.getLogFileName()));
}

TEST(FileLogger, WithIncorrectFilenameFileNotCreated) {
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(std::string("test.log"), flags);
    fileLogger.useRealFiles(true);
    fileLogger.writeToFile("", "", 0, std::ios_base::in | std::ios_base::out);

    EXPECT_FALSE(fileLogger.wasFileCreated(fileLogger.getLogFileName()));
}

TEST(FileLogger, WithCorrectFilenameFileCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(true);
    fileLogger.writeToFile(testFile, "test", 4, std::fstream::out);

    EXPECT_TRUE(fileExists(testFile));
    if (fileExists(testFile)) {
        std::remove(testFile.c_str());
    }
}

TEST(FileLogger, CreatingNewInstanceRemovesOldFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(true);
    fileLogger.writeToFile(fileLogger.getLogFileName(), "test", 4, std::fstream::out);

    EXPECT_TRUE(fileExists(fileLogger.getLogFileName()));
    FullyEnabledFileLogger fileLogger2(testFile, flags);
    EXPECT_FALSE(fileExists(fileLogger.getLogFileName()));
}

TEST(FileLogger, WithDebugFunctionalityDoesNotDumpApiCallLogWhenFlagIsFalseButDumpsCustomLogs) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(false);

    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    fileLogger.logApiCall("searchString", true, 0);
    fileLogger.logApiCall("searchString2", false, 0);
    fileLogger.logInputs("searchString3", "any");
    fileLogger.logInputs("searchString3", "any", "and more");
    fileLogger.log(false, "searchString4");

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_FALSE(str.find("searchString\n") != std::string::npos);
        EXPECT_FALSE(str.find("searchString2\n") != std::string::npos);
        EXPECT_FALSE(str.find("searchString3") != std::string::npos);
        EXPECT_FALSE(str.find("searchString4") != std::string::npos);
    }

    // Log still works
    fileLogger.log(true, "searchString4");

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find("searchString4") != std::string::npos);
    }
}

TEST(FileLogger, WithDebugFunctionalityGetInputReturnsCorectValue) {
    std::string testFile = "testfile";
    DebugVariables flags;

    FullyEnabledFileLogger fileLogger(testFile, flags);

    // getInput returns 0
    size_t input = 5;
    size_t output = fileLogger.getInput(&input, 0);
    EXPECT_EQ(input, output);
}

TEST(FileLogger, WithDebugFunctionalityGetInputNegative) {
    std::string testFile = "testfile";
    DebugVariables flags;

    FullyEnabledFileLogger fileLogger(testFile, flags);
    // getInput returns 0
    size_t output = fileLogger.getInput(nullptr, 2);
    EXPECT_EQ(0u, output);
}

TEST(FileLogger, WithDebugFunctionalityGetSizesReturnsCorectString) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    // getSizes returns string
    uintptr_t input[3] = {1, 2, 3};
    string lwsSizes = fileLogger.getSizes(input, 3, true);
    string gwsSizes = fileLogger.getSizes(input, 3, false);
    string lwsExpected = "localWorkSize[0]: \t1\nlocalWorkSize[1]: \t2\nlocalWorkSize[2]: \t3\n";
    string gwsExpected = "globalWorkSize[0]: \t1\nglobalWorkSize[1]: \t2\nglobalWorkSize[2]: \t3\n";
    EXPECT_EQ(lwsExpected, lwsSizes);
    EXPECT_EQ(gwsExpected, gwsSizes);
}

TEST(FileLogger, WithDebugFunctionalityGetSizesNegative) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    // getSizes returns string
    string lwsSizes = fileLogger.getSizes(nullptr, 3, true);
    string gwsSizes = fileLogger.getSizes(nullptr, 3, false);

    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());
}

TEST(FileLogger, WithoutDebugFunctionalityGetSizesDoesNotReturnString) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyDisabledFileLogger fileLogger(testFile, flags);
    uintptr_t input[3] = {1, 2, 3};
    string lwsSizes = fileLogger.getSizes(input, 3, true);
    string gwsSizes = fileLogger.getSizes(input, 3, false);
    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());
}

TEST(FileLogger, WithDebugFunctionalityGetEventsReturnsCorectString) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    // getEvents returns string
    uintptr_t event = 8;
    uintptr_t *input[3] = {&event, &event, &event};
    string eventsString = fileLogger.getEvents((uintptr_t *)input, 2);
    EXPECT_NE(0u, eventsString.size());
}

TEST(FileLogger, WithDebugFunctionalityGetEventsNegative) {
    std::string testFile = "testfile";
    DebugVariables flags;

    FullyEnabledFileLogger fileLogger(testFile, flags);
    // getEvents returns 0 sized string
    string event = fileLogger.getEvents(nullptr, 2);
    EXPECT_EQ(0u, event.size());
}

TEST(FileLogger, GivenLoggerWithDebugFunctionalityWhenGetMemObjectsIsCalledThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    MockBuffer buffer;
    MemObj *memoryObject = &buffer;
    cl_mem clMem = memoryObject;
    cl_mem clMemObjects[] = {clMem, clMem};
    cl_uint numObjects = 2;

    string memObjectString = fileLogger.getMemObjects(reinterpret_cast<const uintptr_t *>(clMemObjects), numObjects);
    EXPECT_NE(0u, memObjectString.size());
    stringstream output;
    output << "cl_mem " << clMem << ", MemObj " << memoryObject;
    EXPECT_THAT(memObjectString, ::testing::HasSubstr(output.str()));
}

TEST(FileLogger, GivenDebugFunctionalityWhenGetMemObjectsIsCalledWithNullptrThenStringIsEmpty) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);
    string memObjectString = fileLogger.getMemObjects(nullptr, 2);
    EXPECT_EQ(0u, memObjectString.size());
}

TEST(FileLogger, GiveDisabledDebugFunctionalityWhenGetMemObjectsIsCalledThenCallReturnsImmediately) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyDisabledFileLogger fileLogger(testFile, flags);
    string memObjectString = fileLogger.getMemObjects(nullptr, 2);
    EXPECT_EQ(0u, memObjectString.size());
}

TEST(FileLogger, WithDebugFunctionalityDumpKernel) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    string kernelDumpFile = "testDumpKernel";

    // test kernel dumping
    fileLogger.dumpKernel(kernelDumpFile, "kernel source here");
    EXPECT_TRUE(fileLogger.wasFileCreated(kernelDumpFile.append(".txt")));
}

TEST(FileLogger, WithoutDebugFunctionalityDumpKernel) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(false);
    string kernelDumpFile = "testDumpKernel";
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // test kernel dumping
    fileLogger.dumpKernel(kernelDumpFile, "kernel source here");
    EXPECT_FALSE(fileLogger.wasFileCreated(kernelDumpFile.append(".txt")));
}

TEST(FileLogger, WithDebugFunctionalityDumpBinary) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    string programDumpFile = "programBinary.bin";
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    fileLogger.dumpBinaryProgram(1, &length, &ptrBinary);

    EXPECT_TRUE(fileLogger.wasFileCreated(programDumpFile));
}

TEST(FileLogger, WithDebugFunctionalityDumpNullBinary) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    string programDumpFile = "programBinary.bin";
    size_t length = 4;
    fileLogger.dumpBinaryProgram(1, &length, nullptr);

    EXPECT_FALSE(fileLogger.wasFileCreated(programDumpFile));
}

TEST(FileLogger, WithDebugFunctionalityDontDumpKernelsForNullMdi) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs((const MultiDispatchInfo *)nullptr);

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityDontDumpKernelArgsForNullMdi) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs((const MultiDispatchInfo *)nullptr);

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenDebugFunctionalityWhenDebugFlagIsDisabledThenDoNotDumpKernelArgsForMdi) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    auto kernel = unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));
    auto multiDispatchInfo = unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(kernel.get()));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelArgPatchInfo.size = 32;
    kernelArgPatchInfo.sourceOffset = 0;
    kernelArgPatchInfo.crossthreadOffset = 32;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

    size_t crossThreadDataSize = 64;
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(false);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // check if file was created
    std::string expectedFile = "_arg_0_immediate_size_32_flags_0.bin";
    EXPECT_FALSE(fileLogger.wasFileCreated(expectedFile));

    // no files should be created
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelArgsForMdi) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    auto kernel = unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));
    auto multiDispatchInfo = unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(kernel.get()));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelArgPatchInfo.size = 32;
    kernelArgPatchInfo.sourceOffset = 0;
    kernelArgPatchInfo.crossthreadOffset = 32;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

    size_t crossThreadDataSize = 64;
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // check if file was created
    std::string expectedFile = "_arg_0_immediate_size_32_flags_0.bin";
    EXPECT_TRUE(fileLogger.wasFileCreated(expectedFile));

    // file should be created
    EXPECT_EQ(fileLogger.createdFilesCount(), 1);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelNullKernel) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.dumpKernelArgs((const Kernel *)nullptr);

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityAndEmptyKernelDontDumpKernelArgs) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    auto kernel = unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs(kernel.get());

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelArgsImmediate) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    auto kernel = unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelArgPatchInfo.size = 32;
    kernelArgPatchInfo.sourceOffset = 0;
    kernelArgPatchInfo.crossthreadOffset = 32;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

    size_t crossThreadDataSize = 64;
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs(kernel.get());

    // check if file was created
    EXPECT_TRUE(fileLogger.wasFileCreated("_arg_0_immediate_size_32_flags_0.bin"));

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 1);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelArgsImmediateZeroSize) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    auto kernel = unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelArgPatchInfo.size = 0;
    kernelArgPatchInfo.sourceOffset = 0;
    kernelArgPatchInfo.crossthreadOffset = 32;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

    size_t crossThreadDataSize = sizeof(64);
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.dumpKernelArgs(kernel.get());

    // no files should be created for zero size
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelArgsLocalBuffer) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    auto kernel = unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].metadata.addressQualifier = KernelArgMetadata::AddressSpaceQualifier::Local;

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.dumpKernelArgs(kernel.get());

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelArgsBufferNotSet) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].metadataExtended = std::make_unique<NEO::ArgTypeMetadataExtended>();
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].metadataExtended->type = "uint8 *buffer";

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs(kernel.get());

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelArgsBuffer) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto buffer = BufferHelper<>::create(context.get());
    cl_mem clObj = buffer;

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].metadataExtended = std::make_unique<NEO::ArgTypeMetadataExtended>();
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].metadataExtended->type = "uint8 *buffer";
    kernelInfo->kernelArgInfo.at(0).isBuffer = true;

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    kernel->setArg(0, clObj);

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs(kernel.get());

    buffer->release();

    // check if file was created
    EXPECT_TRUE(fileLogger.wasFileCreated("_arg_0_buffer_size_16_flags_1.bin"));

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 1);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelArgsSampler) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].metadataExtended = std::make_unique<NEO::ArgTypeMetadataExtended>();
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].metadataExtended->type = "sampler test";

    kernel->initialize();

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs(kernel.get());

    // no files should be created for sampler arg
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityDumpKernelArgsImageNotSet) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *kernelInfo, *device));

    SKernelBinaryHeaderCommon kernelHeader;
    char surfaceStateHeap[0x80];

    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
    kernelInfo->heapInfo.pSsh = surfaceStateHeap;
    kernelInfo->heapInfo.pKernelHeader = &kernelHeader;
    kernelInfo->usesSsh = true;

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].metadataExtended = std::make_unique<NEO::ArgTypeMetadataExtended>();
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].metadataExtended->type = "image2d buffer";
    kernelInfo->kernelArgInfo[0].isImage = true;
    kernelInfo->kernelArgInfo[0].offsetImgWidth = 0x4;

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.dumpKernelArgs(kernel.get());

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithDebugFunctionalityDumpBinaryNegativeCases) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    size_t length = 1;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    fileLogger.dumpBinaryProgram(1, nullptr, nullptr);
    fileLogger.dumpBinaryProgram(1, nullptr, &ptrBinary);
    fileLogger.dumpBinaryProgram(1, &length, nullptr);
    length = 0;
    fileLogger.dumpBinaryProgram(1, &length, &ptrBinary);
    fileLogger.dumpBinaryProgram(1, &length, nullptr);

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, WithoutDebugFunctionality) {
    string path = ".";
    vector<string> files = Directory::getFiles(path);
    size_t initialNumberOfFiles = files.size();

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    flags.LogApiCalls.set(true);
    flags.DumpKernelArgs.set(true);
    FullyDisabledFileLogger fileLogger(testFile, flags);

    // Should not be enabled without debug functionality
    EXPECT_FALSE(fileLogger.enabled());

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // test kernel dumping
    bool kernelDumpCreated = false;
    string kernelDumpFile = "testDumpKernel";
    fileLogger.dumpKernel(kernelDumpFile, "kernel source here");

    kernelDumpCreated = fileExists(kernelDumpFile.append(".txt"));
    EXPECT_FALSE(kernelDumpCreated);

    // test api logging
    fileLogger.logApiCall(__FUNCTION__, true, 0);
    logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // getInput returns 0
    size_t input = 5;
    size_t output = fileLogger.getInput(&input, 0);
    EXPECT_EQ(0u, output);

    // getEvents returns 0-size string
    string event = fileLogger.getEvents(&input, 0);
    EXPECT_EQ(0u, event.size());

    // getSizes returns 0-size string
    string lwsSizes = fileLogger.getSizes(&input, 0, true);
    string gwsSizes = fileLogger.getSizes(&input, 0, false);
    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());

    // no programDump file
    string programDumpFile = "programBinary.bin";
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    fileLogger.dumpBinaryProgram(1, &length, &ptrBinary);
    EXPECT_FALSE(fileLogger.wasFileCreated(programDumpFile));

    fileLogger.dumpKernelArgs((const Kernel *)nullptr);

    // test api input logging
    fileLogger.logInputs("Arg name", "value");
    fileLogger.logInputs("int", 5);
    logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // check Log
    fileLogger.log(true, "string to be logged");
    logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    files = Directory::getFiles(path);
    size_t finalNumberOfFiles = files.size();

    EXPECT_EQ(initialNumberOfFiles, finalNumberOfFiles);
}

TEST(LoggerApiEnterWrapper, WithDebugFunctionality) {

    const char *name = "function";
    int error = 0;
    {
        auto debugApiWrapper = std::make_unique<TestLoggerApiEnterWrapper<true>>(name, nullptr);
        EXPECT_TRUE(debugApiWrapper->loggedEnter);
    }

    {
        auto debugApiWrapper2 = std::make_unique<TestLoggerApiEnterWrapper<true>>(name, &error);
        EXPECT_TRUE(debugApiWrapper2->loggedEnter);
    }
}

TEST(LoggerApiEnterWrapper, WithoutDebugFunctionality) {

    const char *name = "function";
    int error = 0;
    {
        auto debugApiWrapper = std::make_unique<TestLoggerApiEnterWrapper<false>>(name, nullptr);
        EXPECT_FALSE(debugApiWrapper->loggedEnter);
    }

    {
        auto debugApiWrapper2 = std::make_unique<TestLoggerApiEnterWrapper<false>>(name, &error);
        EXPECT_FALSE(debugApiWrapper2->loggedEnter);
    }
}

TEST(FileLogger, infoPointerToStringReturnsCorrectString) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    uint64_t value64bit = 64;
    string string64bit = fileLogger.infoPointerToString(&value64bit, sizeof(uint64_t));
    uint32_t value32bit = 32;
    string string32bit = fileLogger.infoPointerToString(&value32bit, sizeof(uint32_t));
    uint8_t value8bit = 0;
    string string8bit = fileLogger.infoPointerToString(&value8bit, sizeof(uint8_t));

    EXPECT_STREQ("64", string64bit.c_str());
    EXPECT_STREQ("32", string32bit.c_str());
    EXPECT_STREQ("0", string8bit.c_str());

    string stringNonValue = fileLogger.infoPointerToString(nullptr, 56);
    EXPECT_STREQ("", stringNonValue.c_str());

    char valueChar = 0;
    stringNonValue = fileLogger.infoPointerToString(&valueChar, 56);
    EXPECT_STREQ("", stringNonValue.c_str());
}

TEST(FileLogger, givenDisabledDebugFunctionalityWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsDropped) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyDisabledFileLogger fileLogger(testFile, flags);

    bool wasCalled = false;
    fileLogger.logLazyEvaluateArgs(true, [&] { wasCalled = true; });
    EXPECT_FALSE(wasCalled);
}

TEST(FileLogger, givenDisabledPredicateWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsDropped) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    bool wasCalled = false;
    fileLogger.logLazyEvaluateArgs(false, [&] { wasCalled = true; });
    EXPECT_FALSE(wasCalled);
}

TEST(FileLogger, givenEnabledPredicateWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsExecuted) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);
    bool wasCalled = false;
    fileLogger.logLazyEvaluateArgs(true, [&] { wasCalled = true; });
    EXPECT_TRUE(wasCalled);
}

struct DummyEvaluator {
    DummyEvaluator(bool &wasCalled) {
        wasCalled = true;
    }

    operator const char *() {
        return "";
    }
};

TEST(FileLogger, givenDisabledPredicateWhenDbgLogLazyEvaluateArgsIsCalledThenInputParametersAreNotEvaluated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(false);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    bool wasCalled = false;
    DBG_LOG_LAZY_EVALUATE_ARGS(fileLogger, false, log, true, DummyEvaluator(wasCalled));
    EXPECT_FALSE(wasCalled);
}

TEST(FileLogger, givenEnabledPredicateWhenDbgLogLazyEvaluateArgsIsCalledThenInputParametersAreEvaluated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    bool wasCalled = false;
    DBG_LOG_LAZY_EVALUATE_ARGS(fileLogger, true, log, true, DummyEvaluator(wasCalled));
    EXPECT_TRUE(wasCalled);
}

TEST(FileLogger, whenDisabledThenDebugFunctionalityIsNotAvailableAtCompileTime) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FileLogger<DebugFunctionalityLevel::None> fileLogger(testFile, flags);

    static_assert(false == fileLogger.enabled(), "");
}

TEST(FileLogger, whenFullyEnabledThenAllDebugFunctionalityIsAvailableAtCompileTime) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FileLogger<DebugFunctionalityLevel::Full> fileLogger(testFile, flags);

    static_assert(true == fileLogger.enabled(), "");
}

struct AllocationTypeTestCase {
    GraphicsAllocation::AllocationType type;
    const char *str;
};

AllocationTypeTestCase allocationTypeValues[] = {
    {GraphicsAllocation::AllocationType::BUFFER, "BUFFER"},
    {GraphicsAllocation::AllocationType::BUFFER_COMPRESSED, "BUFFER_COMPRESSED"},
    {GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, "BUFFER_HOST_MEMORY"},
    {GraphicsAllocation::AllocationType::COMMAND_BUFFER, "COMMAND_BUFFER"},
    {GraphicsAllocation::AllocationType::CONSTANT_SURFACE, "CONSTANT_SURFACE"},
    {GraphicsAllocation::AllocationType::DEVICE_QUEUE_BUFFER, "DEVICE_QUEUE_BUFFER"},
    {GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, "EXTERNAL_HOST_PTR"},
    {GraphicsAllocation::AllocationType::FILL_PATTERN, "FILL_PATTERN"},
    {GraphicsAllocation::AllocationType::GLOBAL_SURFACE, "GLOBAL_SURFACE"},
    {GraphicsAllocation::AllocationType::IMAGE, "IMAGE"},
    {GraphicsAllocation::AllocationType::INDIRECT_OBJECT_HEAP, "INDIRECT_OBJECT_HEAP"},
    {GraphicsAllocation::AllocationType::INSTRUCTION_HEAP, "INSTRUCTION_HEAP"},
    {GraphicsAllocation::AllocationType::INTERNAL_HEAP, "INTERNAL_HEAP"},
    {GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY, "INTERNAL_HOST_MEMORY"},
    {GraphicsAllocation::AllocationType::KERNEL_ISA, "KERNEL_ISA"},
    {GraphicsAllocation::AllocationType::LINEAR_STREAM, "LINEAR_STREAM"},
    {GraphicsAllocation::AllocationType::MAP_ALLOCATION, "MAP_ALLOCATION"},
    {GraphicsAllocation::AllocationType::MCS, "MCS"},
    {GraphicsAllocation::AllocationType::PIPE, "PIPE"},
    {GraphicsAllocation::AllocationType::PREEMPTION, "PREEMPTION"},
    {GraphicsAllocation::AllocationType::PRINTF_SURFACE, "PRINTF_SURFACE"},
    {GraphicsAllocation::AllocationType::PRIVATE_SURFACE, "PRIVATE_SURFACE"},
    {GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER, "PROFILING_TAG_BUFFER"},
    {GraphicsAllocation::AllocationType::SCRATCH_SURFACE, "SCRATCH_SURFACE"},
    {GraphicsAllocation::AllocationType::SHARED_BUFFER, "SHARED_BUFFER"},
    {GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE, "SHARED_CONTEXT_IMAGE"},
    {GraphicsAllocation::AllocationType::SHARED_IMAGE, "SHARED_IMAGE"},
    {GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY, "SHARED_RESOURCE_COPY"},
    {GraphicsAllocation::AllocationType::SURFACE_STATE_HEAP, "SURFACE_STATE_HEAP"},
    {GraphicsAllocation::AllocationType::SVM_CPU, "SVM_CPU"},
    {GraphicsAllocation::AllocationType::SVM_GPU, "SVM_GPU"},
    {GraphicsAllocation::AllocationType::SVM_ZERO_COPY, "SVM_ZERO_COPY"},
    {GraphicsAllocation::AllocationType::TAG_BUFFER, "TAG_BUFFER"},
    {GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, "TIMESTAMP_PACKET_TAG_BUFFER"},
    {GraphicsAllocation::AllocationType::UNKNOWN, "UNKNOWN"},
    {GraphicsAllocation::AllocationType::WRITE_COMBINED, "WRITE_COMBINED"}};

class AllocationTypeLogging : public ::testing::TestWithParam<AllocationTypeTestCase> {};

TEST_P(AllocationTypeLogging, givenGraphicsAllocationTypeWhenConvertingToStringThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);
    auto input = GetParam();

    GraphicsAllocation graphicsAllocation(0, input.type, nullptr, 0ull, 0ull, 0, MemoryPool::MemoryNull);

    auto result = fileLogger.getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, input.str);
}

INSTANTIATE_TEST_CASE_P(AllAllocationTypes,
                        AllocationTypeLogging,
                        ::testing::ValuesIn(allocationTypeValues));

TEST(AllocationTypeLoggingSingle, givenGraphicsAllocationTypeWhenConvertingToStringIllegalValueThenILLEGAL_VALUEIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, static_cast<GraphicsAllocation::AllocationType>(999), nullptr, 0ull, 0ull, 0, MemoryPool::MemoryNull);

    auto result = fileLogger.getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, "ILLEGAL_VALUE");
}

TEST(AllocationTypeLoggingSingle, givenDisabledDebugFunctionalityWhenGettingGraphicsAllocationTypeThenNullptrReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyDisabledFileLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, GraphicsAllocation::AllocationType::BUFFER, nullptr, 0ull, 0ull, 0, MemoryPool::MemoryNull);

    auto result = fileLogger.getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, nullptr);
}
