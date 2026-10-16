/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/utilities/file_logger_tests.h"

#include "shared/source/helpers/preprocessor.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/utilities/base_object_utils.h"
#include "shared/test/common/utilities/logger_tests.h"

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <memory>
#include <sstream>
#include <string>

using namespace NEO;

TEST(FileLogger, WhenGettingEventsThenCorrectValueIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile);
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

    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    // getEvents returns 0 sized string
    std::string event = clFileLogger.getEvents(nullptr, 2);
    EXPECT_EQ(0u, event.size());
}

TEST(FileLogger, GivenLoggerWithDebugFunctionalityWhenGetMemObjectsIsCalledThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile);
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
    EXPECT_TRUE(hasSubstr(memObjectString, output.str()));
}

TEST(FileLogger, GivenDebugFunctionalityWhenGetMemObjectsIsCalledWithNullptrThenStringIsEmpty) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    std::string memObjectString = clFileLogger.getMemObjects(nullptr, 2);
    EXPECT_EQ(0u, memObjectString.size());
}

TEST(FileLogger, GiveDisabledDebugFunctionalityWhenGetMemObjectsIsCalledThenCallReturnsImmediately) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyDisabledFileLogger fileLogger(testFile);
    FullyDisabledClFileLogger clFileLogger(fileLogger, flags);
    std::string memObjectString = clFileLogger.getMemObjects(nullptr, 2);
    EXPECT_EQ(0u, memObjectString.size());
}

TEST(FileLogger, GivenNullMdiWhenDumpingKernelsThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
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
    auto crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(false);
    FullyEnabledFileLogger fileLogger(testFile);
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
    auto crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
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
    FullyEnabledFileLogger fileLogger(testFile);
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
    FullyEnabledFileLogger fileLogger(testFile);
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
    auto crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
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
    auto crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
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
    FullyEnabledFileLogger fileLogger(testFile);
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
    auto crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
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
    auto crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    kernel->setArg(0, clObj);

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
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
    FullyEnabledFileLogger fileLogger(testFile);
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
    kernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);

    kernelInfo->addArgImage(0);
    kernelInfo->argAsImg(0).metadataPayload.imgWidth = 0x4;
    kernelInfo->addExtendedMetadata(0, "", "image2d buffer");

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernelArgs.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // no files should be created for local buffer
    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenDisabledDebugFunctionalityWhenLoggingThenDumpingDoesNotOccur) {
    std::string path = "ocl_test";
    std::vector<std::string> files = Directory::getFiles(path);
    size_t initialNumberOfFiles = files.size();

    std::string testFile = "ocl_test/testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    flags.LogApiCalls.set(true);
    flags.DumpKernelArgs.set(true);
    FullyDisabledFileLogger fileLogger(testFile);
    FullyDisabledClFileLogger clFileLogger(fileLogger, flags);

    // Should not be enabled without debug functionality
    EXPECT_FALSE(fileLogger.enabled());

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // test kernel dumping
    bool kernelDumpCreated = false;
    std::string kernelDumpFile = "testDumpKernel";
    clFileLogger.dumpKernel(kernelDumpFile, "kernel source here");

    kernelDumpCreated = virtualFileExists(kernelDumpFile.append(".txt"));
    EXPECT_FALSE(kernelDumpCreated);

    // test api logging
    clFileLogger.logApiCall(NEO_FUNCTION_NAME, true, 0);
    logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // getInput returns 0
    size_t input = 5;
    size_t output = clFileLogger.getInput(&input, 0);
    EXPECT_EQ(0u, output);

    // getEvents returns 0-size string
    std::string event = clFileLogger.getEvents(&input, 0);
    EXPECT_EQ(0u, event.size());

    // getSizes returns 0-size string
    std::string lwsSizes = clFileLogger.getSizes(&input, 0, true);
    std::string gwsSizes = clFileLogger.getSizes(&input, 0, false);
    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());

    EXPECT_EQ(0u, clFileLogger.infoPointerToString(&input, sizeof(input)).size());

    // no programDump file
    std::string programDumpFile = "programBinary.bin";
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    clFileLogger.dumpBinaryProgram(1, &length, &ptrBinary);
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
    auto crossThreadData = std::make_unique<uint8_t[]>(crossThreadDataSize);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    kernel->setArg(0, nullptr);
    clFileLogger.dumpKernelArgs(multiDispatchInfo.get());

    // test api input logging
    clFileLogger.logInputs("Arg name", "value");
    clFileLogger.logInputs("int", 5);
    logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // check Log
    fileLogger.log(true, "string to be logged");
    logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    files = Directory::getFiles(path);
    size_t finalNumberOfFiles = files.size();

    EXPECT_EQ(initialNumberOfFiles, finalNumberOfFiles);
}

TEST(FileLogger, GivenEnabledDebugFunctinalityWhenLoggingApiCallsThenDumpToFile) {
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(std::string("test.log"));
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    clFileLogger.logApiCall("searchString", true, 0);
    clFileLogger.logApiCall("searchString2", false, 0);
    clFileLogger.logInputs("searchString3", "any");
    clFileLogger.logInputs("searchString3", "any", "and more");
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

    FullyDisabledFileLogger fileLogger(std::string("  "));

    FullyDisabledClFileLogger clFileLogger(fileLogger, flags);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    clFileLogger.logApiCall("searchString", true, 0);
    clFileLogger.logApiCall("searchString2", false, 0);
    clFileLogger.logInputs("searchString3", "any");
    clFileLogger.logInputs("searchString3", "any", "and more");
    fileLogger.log(false, "searchString4");

    EXPECT_FALSE(fileLogger.wasFileCreated(fileLogger.getLogFileName()));
}

TEST(FileLogger, GivenFlagIsFalseWhenLoggingThenOnlyCustomLogsAreDumped) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(false);

    FullyEnabledFileLogger fileLogger(testFile);

    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    clFileLogger.logApiCall("searchString", true, 0);
    clFileLogger.logApiCall("searchString2", false, 0);
    clFileLogger.logInputs("searchString3", "any");
    clFileLogger.logInputs("searchString3", "any", "and more");
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

    FullyEnabledFileLogger fileLogger(testFile);

    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    // getInput returns 0
    size_t input = 5;
    size_t output = clFileLogger.getInput(&input, 0);
    EXPECT_EQ(input, output);
}

TEST(FileLogger, GivenNullInputWhenGettingInputThenZeroIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;

    FullyEnabledFileLogger fileLogger(testFile);

    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    // getInput returns 0
    size_t output = clFileLogger.getInput(nullptr, 2);
    EXPECT_EQ(0u, output);
}

TEST(FileLogger, WhenGettingSizesThenCorrectValueIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile);

    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    // getSizes returns string
    uintptr_t input[3] = {1, 2, 3};
    std::string lwsSizes = clFileLogger.getSizes(input, 3, true);
    std::string gwsSizes = clFileLogger.getSizes(input, 3, false);
    std::string lwsExpected = "localWorkSize[0]: \t1\nlocalWorkSize[1]: \t2\nlocalWorkSize[2]: \t3\n";
    std::string gwsExpected = "globalWorkSize[0]: \t1\nglobalWorkSize[1]: \t2\nglobalWorkSize[2]: \t3\n";
    EXPECT_EQ(lwsExpected, lwsSizes);
    EXPECT_EQ(gwsExpected, gwsSizes);
}

TEST(FileLogger, GivenNullInputWhenGettingSizesThenZeroIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyEnabledFileLogger fileLogger(testFile);

    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    // getSizes returns string
    std::string lwsSizes = clFileLogger.getSizes(nullptr, 3, true);
    std::string gwsSizes = clFileLogger.getSizes(nullptr, 3, false);

    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());
}

TEST(FileLogger, GivenDisabledDebugFunctionalityWhenGettingSizesThenEmptyStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);

    FullyDisabledFileLogger fileLogger(testFile);

    FullyDisabledClFileLogger clFileLogger(fileLogger, flags);
    uintptr_t input[3] = {1, 2, 3};
    std::string lwsSizes = clFileLogger.getSizes(input, 3, true);
    std::string gwsSizes = clFileLogger.getSizes(input, 3, false);
    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());
}

TEST(FileLogger, WhenDumpingKernelThenFileIsCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    std::string kernelDumpFile = "testDumpKernel";

    // test kernel dumping
    clFileLogger.dumpKernel(kernelDumpFile, "kernel source here");
    EXPECT_TRUE(fileLogger.wasFileCreated(kernelDumpFile.append(".txt")));
}

TEST(FileLogger, GivenDisabledDebugFunctionalityWhenDumpingKernelThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(false);
    std::string kernelDumpFile = "testDumpKernel";
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    // test kernel dumping
    clFileLogger.dumpKernel(kernelDumpFile, "kernel source here");
    EXPECT_FALSE(fileLogger.wasFileCreated(kernelDumpFile.append(".txt")));
}

TEST(FileLogger, GivenDumpKernelsDisabledWhenDumpingBinaryFileThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(false);
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;

    clFileLogger.dumpBinaryProgram(1, &length, &ptrBinary);

    EXPECT_FALSE(fileLogger.wasFileCreated("programBinary.bin"));
}

TEST(FileLogger, WhenDumpingBinaryFileThenFileIsCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    std::string programDumpFile = "programBinary.bin";
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    clFileLogger.dumpBinaryProgram(1, &length, &ptrBinary);

    EXPECT_TRUE(fileLogger.wasFileCreated(programDumpFile));
}

TEST(FileLogger, GivenNullPointerWhenDumpingBinaryFileThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    std::string programDumpFile = "programBinary.bin";
    size_t length = 4;
    clFileLogger.dumpBinaryProgram(1, &length, nullptr);

    EXPECT_FALSE(fileLogger.wasFileCreated(programDumpFile));
}

TEST(FileLogger, GivenNullsWhenDumpingKernelArgsThenFileIsNotCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);
    size_t length = 1;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    clFileLogger.dumpBinaryProgram(1, nullptr, nullptr);
    clFileLogger.dumpBinaryProgram(1, nullptr, &ptrBinary);
    clFileLogger.dumpBinaryProgram(1, &length, nullptr);
    const unsigned char *nullBinary = nullptr;
    clFileLogger.dumpBinaryProgram(1, &length, &nullBinary);
    length = 0;
    clFileLogger.dumpBinaryProgram(1, &length, &ptrBinary);
    clFileLogger.dumpBinaryProgram(1, &length, nullptr);

    EXPECT_EQ(fileLogger.createdFilesCount(), 0);
}

TEST(FileLogger, GivenMisalignedValueWhenConvertingInfoPointerToStringThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    alignas(uint64_t) unsigned char buffer[sizeof(uint64_t) + 1] = {};
    const uint64_t value64bit = 64;
    memcpy_s(buffer + 1, sizeof(value64bit), &value64bit, sizeof(value64bit));

    EXPECT_STREQ("64", clFileLogger.infoPointerToString(buffer + 1, sizeof(uint64_t)).c_str());
}

TEST(FileLogger, WhenConvertingInfoPointerToStringThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile);
    FullyEnabledClFileLogger clFileLogger(fileLogger, flags);

    uint64_t value64bit = 64;
    std::string string64bit = clFileLogger.infoPointerToString(&value64bit, sizeof(uint64_t));
    uint32_t value32bit = 32;
    std::string string32bit = clFileLogger.infoPointerToString(&value32bit, sizeof(uint32_t));
    uint8_t value8bit = 0;
    std::string string8bit = clFileLogger.infoPointerToString(&value8bit, sizeof(uint8_t));

    EXPECT_STREQ("64", string64bit.c_str());
    EXPECT_STREQ("32", string32bit.c_str());
    EXPECT_STREQ("0", string8bit.c_str());

    std::string stringNonValue = clFileLogger.infoPointerToString(nullptr, 56);
    EXPECT_STREQ("", stringNonValue.c_str());

    char valueChar = 0;
    stringNonValue = clFileLogger.infoPointerToString(&valueChar, 56);
    EXPECT_STREQ("", stringNonValue.c_str());
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
