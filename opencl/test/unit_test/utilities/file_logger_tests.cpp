/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/utilities/file_logger_tests.h"

#include "shared/source/utilities/logger.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>

using namespace NEO;

TEST(FileLogger, WhenFileLoggerIsCreatedThenItIsEnabled) {
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

TEST(FileLogger, GivenEnabledDebugFunctinalityWhenLoggingApiCallsThenDumpToFile) {
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

TEST(FileLogger, GivenDisabledDebugFunctinalityWhenLoggingApiCallsThenFileIsNotCreated) {
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

TEST(FileLogger, GivenIncorrectFilenameFileWhenLoggingApiCallsThenFileIsNotCreated) {
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(std::string("test.log"), flags);
    fileLogger.useRealFiles(true);
    fileLogger.writeToFile("", "", 0, std::ios_base::in | std::ios_base::out);

    EXPECT_FALSE(fileLogger.wasFileCreated(fileLogger.getLogFileName()));
}

TEST(FileLogger, GivenCorrectFilenameFileWhenLoggingApiCallsThenFileIsCreated) {
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

TEST(FileLogger, GivenSameFileNameWhenCreatingNewInstanceThenOldFileIsRemoved) {
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

TEST(FileLogger, GivenFlagIsFalseWhenLoggingThenOnlyCustomLogsAreDumped) {
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

TEST(FileLogger, WhenGettingInputThenCorrectValueIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;

    FullyEnabledFileLogger fileLogger(testFile, flags);

    // getInput returns 0
    size_t input = 5;
    size_t output = fileLogger.getInput(&input, 0);
    EXPECT_EQ(input, output);
}

TEST(FileLogger, GivenNullInputWhenGettingInputThenZeroIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;

    FullyEnabledFileLogger fileLogger(testFile, flags);
    // getInput returns 0
    size_t output = fileLogger.getInput(nullptr, 2);
    EXPECT_EQ(0u, output);
}

TEST(FileLogger, WhenGettingSizesThenCorrectValueIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    // getSizes returns string
    uintptr_t input[3] = {1, 2, 3};
    std::string lwsSizes = fileLogger.getSizes(input, 3, true);
    std::string gwsSizes = fileLogger.getSizes(input, 3, false);
    std::string lwsExpected = "localWorkSize[0]: \t1\nlocalWorkSize[1]: \t2\nlocalWorkSize[2]: \t3\n";
    std::string gwsExpected = "globalWorkSize[0]: \t1\nglobalWorkSize[1]: \t2\nglobalWorkSize[2]: \t3\n";
    EXPECT_EQ(lwsExpected, lwsSizes);
    EXPECT_EQ(gwsExpected, gwsSizes);
}

TEST(FileLogger, GivenNullInputWhenGettingSizesThenZeroIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    // getSizes returns string
    std::string lwsSizes = fileLogger.getSizes(nullptr, 3, true);
    std::string gwsSizes = fileLogger.getSizes(nullptr, 3, false);

    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());
}

TEST(FileLogger, GivenDisabledDebugFunctionalityWhenGettingSizesThenEmptyStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyDisabledFileLogger fileLogger(testFile, flags);
    uintptr_t input[3] = {1, 2, 3};
    std::string lwsSizes = fileLogger.getSizes(input, 3, true);
    std::string gwsSizes = fileLogger.getSizes(input, 3, false);
    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());
}

TEST(FileLogger, WhenGettingEventsThenCorrectValueIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    // getEvents returns string
    uintptr_t event = 8;
    uintptr_t *input[3] = {&event, &event, &event};
    std::string eventsString = clFileLogger.getEvents((uintptr_t *)input, 2);
    EXPECT_NE(0u, eventsString.size());
}

TEST(FileLogger, GivenNullInputWhenGettingEventsThenZeroIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;

    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    // getEvents returns 0 sized string
    std::string event = clFileLogger.getEvents(nullptr, 2);
    EXPECT_EQ(0u, event.size());
}

TEST(FileLogger, GivenLoggerWithDebugFunctionalityWhenGetMemObjectsIsCalledThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    MockBuffer buffer;
    MemObj *memoryObject = &buffer;
    cl_mem clMem = memoryObject;
    cl_mem clMemObjects[] = {clMem, clMem};
    cl_uint numObjects = 2;

    std::string memObjectString = clFileLogger.getMemObjects(reinterpret_cast<const uintptr_t *>(clMemObjects), numObjects);
    EXPECT_NE(0u, memObjectString.size());
    std::stringstream output;
    output << "cl_mem " << clMem << ", MemObj " << memoryObject;
    EXPECT_THAT(memObjectString, ::testing::HasSubstr(output.str()));
}

TEST(FileLogger, GivenDebugFunctionalityWhenGetMemObjectsIsCalledWithNullptrThenStringIsEmpty) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    std::string memObjectString = clFileLogger.getMemObjects(nullptr, 2);
    EXPECT_EQ(0u, memObjectString.size());
}

TEST(FileLogger, GiveDisabledDebugFunctionalityWhenGetMemObjectsIsCalledThenCallReturnsImmediately) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyDisabledFileLogger fileLogger(testFile, flags);
    FullyDisabledClFileLogger clFileLogger(fileLogger, flags);
    std::string memObjectString = clFileLogger.getMemObjects(nullptr, 2);
    EXPECT_EQ(0u, memObjectString.size());
}

TEST(FileLogger, WhenDumpingKernelThenFileIsCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    std::string kernelDumpFile = "testDumpKernel";

    // test kernel dumping
    fileLogger.dumpKernel(kernelDumpFile, "kernel source here");
    EXPECT_TRUE(fileLogger.wasFileCreated(kernelDumpFile.append(".txt")));
}

TEST(FileLogger, GivenDisabledDebugFunctionalityWhenDumpingKernelThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(false);
    std::string kernelDumpFile = "testDumpKernel";
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // test kernel dumping
    fileLogger.dumpKernel(kernelDumpFile, "kernel source here");
    EXPECT_FALSE(fileLogger.wasFileCreated(kernelDumpFile.append(".txt")));
}

TEST(FileLogger, WhenDumpingBinaryFileThenFileIsCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    std::string programDumpFile = "programBinary.bin";
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    fileLogger.dumpBinaryProgram(1, &length, &ptrBinary);

    EXPECT_TRUE(fileLogger.wasFileCreated(programDumpFile));
}

TEST(FileLogger, GivenNullPointerWhenDumpingBinaryFileThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    std::string programDumpFile = "programBinary.bin";
    size_t length = 4;
    fileLogger.dumpBinaryProgram(1, &length, nullptr);

    EXPECT_FALSE(fileLogger.wasFileCreated(programDumpFile));
}

TEST(FileLogger, GivenNullMdiWhenDumpingKernelsThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(nullptr);

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenDebugFunctionalityWhenDebugFlagIsDisabledThenDoNotDumpKernelArgsForMdi) {
    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    auto kernel = std::unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgImmediate(0, 32, 32);

    size_t crossThreadDataSize = 64;
    auto crossThreadData = std::unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(false);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // check if file was created
    std::string expectedFile = "_arg_0_immediate_size_32_flags_0.bin";
    EXPECT_FALSE(fileLogger.wasFileCreated(expectedFile));

    // no files should be created
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenMdiWhenDumpingKernelArgsThenFileIsCreated) {
    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    auto kernel = std::unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgImmediate(0, 32, 32);

    size_t crossThreadDataSize = 64;
    auto crossThreadData = std::unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // check if file was created
    std::string expectedFile = "_arg_0_immediate_size_32_flags_0.bin";
    EXPECT_TRUE(fileLogger.wasFileCreated(expectedFile));

    // file should be created
    EXPECT_EQ(fileLogger.createdFilesCount(), 1);
}

TEST(FileLogger, GivenNullWhenDumpingKernelArgsThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), nullptr));
    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenEmptyKernelWhenDumpingKernelArgsThenFileIsNotCreated) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    auto kernel = std::unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenImmediateWhenDumpingKernelArgsThenFileIsCreated) {
    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    auto kernel = std::unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgImmediate(0, 32, 32);

    size_t crossThreadDataSize = 64;
    auto crossThreadData = std::unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // check if file was created
    EXPECT_TRUE(fileLogger.wasFileCreated("_arg_0_immediate_size_32_flags_0.bin"));

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 1);
}

TEST(FileLogger, GivenImmediateZeroSizeWhenDumpingKernelArgsThenFileIsNotCreated) {
    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    auto kernel = std::unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgImmediate(0, 0, 32);

    size_t crossThreadDataSize = sizeof(64);
    auto crossThreadData = std::unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // no files should be created for zero size
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenLocalBufferWhenDumpingKernelArgsThenFileIsNotCreated) {
    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(toClDeviceVector(*device));
    auto kernel = std::unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgBuffer(0);
    kernelInfo->setAddressQualifier(0, KernelArgMetadata::AddrLocal);

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenBufferNotSetWhenDumpingKernelArgsThenFileIsNotCreated) {
    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    auto kernel = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgBuffer(0);
    kernelInfo->addExtendedMetadata(0, "", "uint8 *buffer");

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = std::unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenBufferWhenDumpingKernelArgsThenFileIsCreated) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto buffer = BufferHelper<>::create(context.get());
    cl_mem clObj = buffer;

    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    auto kernel = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgBuffer(0);
    kernelInfo->addExtendedMetadata(0, "", "uint8 *buffer");

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = std::unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    kernel->setArg(0, clObj);

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    buffer->release();

    // check if file was created
    EXPECT_TRUE(fileLogger.wasFileCreated("_arg_0_buffer_size_16_flags_1.bin"));

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 1);
}

TEST(FileLogger, GivenSamplerWhenDumpingKernelArgsThenFileIsNotCreated) {
    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    auto kernel = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgSampler(0);
    kernelInfo->addExtendedMetadata(0, "", "sampler test");

    kernel->initialize();

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // no files should be created for sampler arg
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenImageNotSetWhenDumpingKernelArgsThenFileIsNotCreated) {
    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    auto kernel = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    char surfaceStateHeap[0x80];
    kernelInfo->heapInfo.pSsh = surfaceStateHeap;
    kernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    kernelInfo->addArgImage(0);
    kernelInfo->argAsImg(0).metadataPayload.imgWidth = 0x4;
    kernelInfo->addExtendedMetadata(0, "", "image2d buffer");

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = std::unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenNullsWhenDumpingKernelArgsThenFileIsNotCreated) {
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

TEST(FileLogger, GivenDisabledDebugFunctionalityWhenLoggingThenDumpingDoesNotOccur) {
    std::string path = ".";
    std::vector<std::string> files = Directory::getFiles(path);
    size_t initialNumberOfFiles = files.size();

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    flags.LogApiCalls.set(true);
    flags.DumpKernelArgs.set(true);
    FullyDisabledFileLogger fileLogger(testFile, flags);
    FullyDisabledClFileLogger clFileLogger(fileLogger, flags);

    // Should not be enabled without debug functionality
    EXPECT_FALSE(fileLogger.enabled());

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // test kernel dumping
    bool kernelDumpCreated = false;
    std::string kernelDumpFile = "testDumpKernel";
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
    std::string event = clFileLogger.getEvents(&input, 0);
    EXPECT_EQ(0u, event.size());

    // getSizes returns 0-size string
    std::string lwsSizes = fileLogger.getSizes(&input, 0, true);
    std::string gwsSizes = fileLogger.getSizes(&input, 0, false);
    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());

    // no programDump file
    std::string programDumpFile = "programBinary.bin";
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    fileLogger.dumpBinaryProgram(1, &length, &ptrBinary);
    EXPECT_FALSE(fileLogger.wasFileCreated(programDumpFile));

    auto kernelInfo = std::make_unique<MockKernelInfo>();
    kernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    auto kernel = std::make_unique<MockKernel>(program.get(), *kernelInfo, *device);
    auto multiDispatchInfo = std::unique_ptr<MockMultiDispatchInfo>(new MockMultiDispatchInfo(device.get(), kernel.get()));

    kernelInfo->addArgBuffer(0);
    kernelInfo->addExtendedMetadata(0, "", "uint8 *buffer");

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = std::unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    kernel->setArg(0, nullptr);
    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

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

TEST(LoggerApiEnterWrapper, GivenDebugFunctionalityEnabledWhenApiWrapperIsCreatedThenEntryIsLogged) {

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

TEST(LoggerApiEnterWrapper, GivenDebugFunctionalityDisabledWhenApiWrapperIsCreatedThenEntryIsNotLogged) {

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

TEST(FileLogger, WhenConvertingInfoPointerToStringThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    uint64_t value64bit = 64;
    std::string string64bit = fileLogger.infoPointerToString(&value64bit, sizeof(uint64_t));
    uint32_t value32bit = 32;
    std::string string32bit = fileLogger.infoPointerToString(&value32bit, sizeof(uint32_t));
    uint8_t value8bit = 0;
    std::string string8bit = fileLogger.infoPointerToString(&value8bit, sizeof(uint8_t));

    EXPECT_STREQ("64", string64bit.c_str());
    EXPECT_STREQ("32", string32bit.c_str());
    EXPECT_STREQ("0", string8bit.c_str());

    std::string stringNonValue = fileLogger.infoPointerToString(nullptr, 56);
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
    AllocationType type;
    const char *str;
};

AllocationTypeTestCase allocationTypeValues[] = {
    {AllocationType::BUFFER, "BUFFER"},
    {AllocationType::BUFFER_HOST_MEMORY, "BUFFER_HOST_MEMORY"},
    {AllocationType::COMMAND_BUFFER, "COMMAND_BUFFER"},
    {AllocationType::CONSTANT_SURFACE, "CONSTANT_SURFACE"},
    {AllocationType::EXTERNAL_HOST_PTR, "EXTERNAL_HOST_PTR"},
    {AllocationType::FILL_PATTERN, "FILL_PATTERN"},
    {AllocationType::GLOBAL_SURFACE, "GLOBAL_SURFACE"},
    {AllocationType::IMAGE, "IMAGE"},
    {AllocationType::INDIRECT_OBJECT_HEAP, "INDIRECT_OBJECT_HEAP"},
    {AllocationType::INSTRUCTION_HEAP, "INSTRUCTION_HEAP"},
    {AllocationType::INTERNAL_HEAP, "INTERNAL_HEAP"},
    {AllocationType::INTERNAL_HOST_MEMORY, "INTERNAL_HOST_MEMORY"},
    {AllocationType::KERNEL_ISA, "KERNEL_ISA"},
    {AllocationType::KERNEL_ISA_INTERNAL, "KERNEL_ISA_INTERNAL"},
    {AllocationType::LINEAR_STREAM, "LINEAR_STREAM"},
    {AllocationType::MAP_ALLOCATION, "MAP_ALLOCATION"},
    {AllocationType::MCS, "MCS"},
    {AllocationType::PIPE, "PIPE"},
    {AllocationType::PREEMPTION, "PREEMPTION"},
    {AllocationType::PRINTF_SURFACE, "PRINTF_SURFACE"},
    {AllocationType::PRIVATE_SURFACE, "PRIVATE_SURFACE"},
    {AllocationType::PROFILING_TAG_BUFFER, "PROFILING_TAG_BUFFER"},
    {AllocationType::SCRATCH_SURFACE, "SCRATCH_SURFACE"},
    {AllocationType::WORK_PARTITION_SURFACE, "WORK_PARTITION_SURFACE"},
    {AllocationType::SHARED_BUFFER, "SHARED_BUFFER"},
    {AllocationType::SHARED_CONTEXT_IMAGE, "SHARED_CONTEXT_IMAGE"},
    {AllocationType::SHARED_IMAGE, "SHARED_IMAGE"},
    {AllocationType::SHARED_RESOURCE_COPY, "SHARED_RESOURCE_COPY"},
    {AllocationType::SURFACE_STATE_HEAP, "SURFACE_STATE_HEAP"},
    {AllocationType::SVM_CPU, "SVM_CPU"},
    {AllocationType::SVM_GPU, "SVM_GPU"},
    {AllocationType::SVM_ZERO_COPY, "SVM_ZERO_COPY"},
    {AllocationType::TAG_BUFFER, "TAG_BUFFER"},
    {AllocationType::GLOBAL_FENCE, "GLOBAL_FENCE"},
    {AllocationType::TIMESTAMP_PACKET_TAG_BUFFER, "TIMESTAMP_PACKET_TAG_BUFFER"},
    {AllocationType::UNKNOWN, "UNKNOWN"},
    {AllocationType::WRITE_COMBINED, "WRITE_COMBINED"},
    {AllocationType::DEBUG_CONTEXT_SAVE_AREA, "DEBUG_CONTEXT_SAVE_AREA"},
    {AllocationType::DEBUG_SBA_TRACKING_BUFFER, "DEBUG_SBA_TRACKING_BUFFER"},
    {AllocationType::DEBUG_MODULE_AREA, "DEBUG_MODULE_AREA"},
    {AllocationType::SW_TAG_BUFFER, "SW_TAG_BUFFER"}};

class AllocationTypeLogging : public ::testing::TestWithParam<AllocationTypeTestCase> {};

TEST_P(AllocationTypeLogging, givenGraphicsAllocationTypeWhenConvertingToStringThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);
    auto input = GetParam();

    GraphicsAllocation graphicsAllocation(0, input.type, nullptr, 0ull, 0ull, 0, MemoryPool::MemoryNull);

    auto result = getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, input.str);
}

INSTANTIATE_TEST_CASE_P(AllAllocationTypes,
                        AllocationTypeLogging,
                        ::testing::ValuesIn(allocationTypeValues));

TEST(AllocationTypeLoggingSingle, givenGraphicsAllocationTypeWhenConvertingToStringIllegalValueThenILLEGAL_VALUEIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, static_cast<AllocationType>(999), nullptr, 0ull, 0ull, 0, MemoryPool::MemoryNull);

    auto result = getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, "ILLEGAL_VALUE");
}

TEST(AllocationTypeLoggingSingle, givenAllocationTypeWhenConvertingToStringThenSupportAll) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, AllocationType::UNKNOWN, nullptr, 0ull, 0ull, 0, MemoryPool::MemoryNull);

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::COUNT); i++) {
        graphicsAllocation.setAllocationType(static_cast<AllocationType>(i));

        auto result = getAllocationTypeString(&graphicsAllocation);

        EXPECT_STRNE(result, "ILLEGAL_VALUE");
    }
}

TEST(AllocationTypeLoggingSingle, givenDebugVariableToCaptureAllocationTypeWhenFunctionIsCalledThenProperAllocationTypeIsPrinted) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationType.set(1);

    FullyEnabledFileLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, AllocationType::COMMAND_BUFFER, nullptr, 0ull, 0ull, 0, MemoryPool::MemoryNull);

    testing::internal::CaptureStdout();
    fileLogger.logAllocation(&graphicsAllocation);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput = "Created Graphics Allocation of type COMMAND_BUFFER\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.c_str());
}

TEST(AllocationTypeLoggingSingle, givenLogAllocationTypeWhenLoggingAllocationThenTypeIsLoggedToFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationType.set(1);

    FullyEnabledFileLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, AllocationType::COMMAND_BUFFER, nullptr, 0ull, 0ull, 0, MemoryPool::MemoryNull);

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    testing::internal::CaptureStdout();
    fileLogger.logAllocation(&graphicsAllocation);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput = "Created Graphics Allocation of type COMMAND_BUFFER\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.c_str());

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find("AllocationType: ") != std::string::npos);
    } else {
        EXPECT_FALSE(true);
    }
}
