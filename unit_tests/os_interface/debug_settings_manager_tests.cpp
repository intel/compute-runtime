/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/debug_file_reader.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/os_interface/debug_settings_manager_fixture.h"
#include "unit_tests/utilities/base_object_utils.h"

#include <cstdio>
#include <memory>
#include <sstream>
#include <string>

TEST(DebugSettingsManager, WithDebugFunctionality) {
    FullyEnabledTestDebugManager debugManager;

    EXPECT_FALSE(debugManager.disabled());

    EXPECT_EQ(nullptr, debugManager.injectFcn);
}

TEST(DebugSettingsManager, WithDebugFunctionalityHasSettingsReader) {
    FullyEnabledTestDebugManager debugManager;
    // SettingsReader created
    EXPECT_NE(nullptr, debugManager.getSettingsReader());
}

TEST(DebugSettingsManager, WithDebugFunctionalityCreatesAndDumpsToLogFile) {
    string settings = "LogApiCalls = 1";
    SettingsFileCreator settingsFile(settings);

    FullyEnabledTestDebugManager debugManager;
    if (debugManager.registryReadAvailable()) {
        debugManager.setReaderImpl(SettingsReader::create());
        debugManager.injectSettingsFromReader();
    }
    debugManager.logApiCall("searchString", true, 0);
    debugManager.logApiCall("searchString2", false, 0);
    debugManager.logInputs("searchString3", "any");
    debugManager.logInputs("searchString3", "any", "and more");
    debugManager.log(false, "searchString4");

    // Log file not created
    EXPECT_TRUE(debugManager.wasFileCreated(debugManager.getLogFileName()));

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_TRUE(str.find("searchString") != std::string::npos);
        EXPECT_TRUE(str.find("searchString2") != std::string::npos);
        EXPECT_TRUE(str.find("searchString3") != std::string::npos);
        EXPECT_FALSE(str.find("searchString4") != std::string::npos);
    }

    debugManager.log(true, "searchString4");

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_TRUE(str.find("searchString4") != std::string::npos);
    }
}

TEST(DebugSettingsManager, WithoutDebugFunctionalityDoesNotCreateLogFile) {
    string settings = "LogApiCalls = 1";
    SettingsFileCreator settingsFile(settings);

    FullyDisabledTestDebugManager debugManager;

    debugManager.setLogFileName("  ");
    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    debugManager.logApiCall("searchString", true, 0);
    debugManager.logApiCall("searchString2", false, 0);
    debugManager.logInputs("searchString3", "any");
    debugManager.logInputs("searchString3", "any", "and more");
    debugManager.log(false, "searchString4");

    EXPECT_FALSE(debugManager.wasFileCreated(debugManager.getLogFileName()));
}

TEST(DebugSettingsManager, WithIncorrectFilenameFileNotCreated) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.useRealFiles(true);
    debugManager.writeToFile("", "", 0, std::ios_base::in | std::ios_base::out);
}

TEST(DebugSettingsManager, WithCorrectFilenameFileCreated) {
    std::string testFile = "testfile";

    FullyEnabledTestDebugManager debugManager;
    debugManager.useRealFiles(true);
    debugManager.writeToFile(testFile, "test", 4, std::fstream::out);

    EXPECT_TRUE(fileExists(testFile));
    if (fileExists(testFile)) {
        std::remove(testFile.c_str());
    }
}

TEST(DebugSettingsManager, CreatingNewInstanceRemovesOldFile) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.useRealFiles(true);
    debugManager.writeToFile(debugManager.getLogFileName(), "test", 4, std::fstream::out);

    EXPECT_TRUE(fileExists(debugManager.getLogFileName()));
    FullyEnabledTestDebugManager debugManager2;
    EXPECT_FALSE(fileExists(debugManager.getLogFileName()));
}

TEST(DebugSettingsManager, WithDebugFunctionalityDoesNotDumpApiCallLogWhenFlagIsFalseButDumpsCustomLogs) {
    FullyEnabledTestDebugManager debugManager;

    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    debugManager.flags.LogApiCalls.set(false);

    debugManager.logApiCall("searchString", true, 0);
    debugManager.logApiCall("searchString2", false, 0);
    debugManager.logInputs("searchString3", "any");
    debugManager.logInputs("searchString3", "any", "and more");
    debugManager.log(false, "searchString4");

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_FALSE(str.find("searchString") != std::string::npos);
        EXPECT_FALSE(str.find("searchString2") != std::string::npos);
        EXPECT_FALSE(str.find("searchString3") != std::string::npos);
        EXPECT_FALSE(str.find("searchString4") != std::string::npos);
    }

    // Log still works
    debugManager.log(true, "searchString4");

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_TRUE(str.find("searchString4") != std::string::npos);
    }
}

TEST(DebugSettingsManager, WithDebugFunctionalityGetInputReturnsCorectValue) {
    FullyEnabledTestDebugManager debugManager;
    // getInput returns 0
    size_t input = 5;
    size_t output = debugManager.getInput(&input, 0);
    EXPECT_EQ(input, output);
}

TEST(DebugSettingsManager, WithDebugFunctionalityGetInputNegative) {
    FullyEnabledTestDebugManager debugManager;
    // getInput returns 0
    size_t output = debugManager.getInput(nullptr, 2);
    EXPECT_EQ(0u, output);
}

TEST(DebugSettingsManager, WithDebugFunctionalityGetSizesReturnsCorectString) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.LogApiCalls.set(true);
    // getSizes returns string
    uintptr_t input[3] = {1, 2, 3};
    string lwsSizes = debugManager.getSizes(input, 3, true);
    string gwsSizes = debugManager.getSizes(input, 3, false);
    string lwsExpected = "localWorkSize[0]: \t1\nlocalWorkSize[1]: \t2\nlocalWorkSize[2]: \t3\n";
    string gwsExpected = "globalWorkSize[0]: \t1\nglobalWorkSize[1]: \t2\nglobalWorkSize[2]: \t3\n";
    EXPECT_EQ(lwsExpected, lwsSizes);
    EXPECT_EQ(gwsExpected, gwsSizes);
}

TEST(DebugSettingsManager, WithDebugFunctionalityGetSizesNegative) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.LogApiCalls.set(true);
    // getSizes returns string
    string lwsSizes = debugManager.getSizes(nullptr, 3, true);
    string gwsSizes = debugManager.getSizes(nullptr, 3, false);

    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());
}

TEST(DebugSettingsManager, WithoutDebugFunctionalityGetSizesDoesNotReturnString) {
    FullyDisabledTestDebugManager debugManager;
    debugManager.flags.LogApiCalls.set(true);
    uintptr_t input[3] = {1, 2, 3};
    string lwsSizes = debugManager.getSizes(input, 3, true);
    string gwsSizes = debugManager.getSizes(input, 3, false);
    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());
}

TEST(DebugSettingsManager, WithDebugFunctionalityGetEventsReturnsCorectString) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.LogApiCalls.set(true);
    // getEvents returns string
    uintptr_t event = 8;
    uintptr_t *input[3] = {&event, &event, &event};
    string eventsString = debugManager.getEvents((uintptr_t *)input, 2);
    EXPECT_NE(0u, eventsString.size());
}

TEST(DebugSettingsManager, WithDebugFunctionalityGetEventsNegative) {
    FullyEnabledTestDebugManager debugManager;
    // getEvents returns 0 sized string
    string event = debugManager.getEvents(nullptr, 2);
    EXPECT_EQ(0u, event.size());
}

TEST(DebugSettingsManager, GivenDebugManagerWithDebugFunctionalityWhenGetMemObjectsIsCalledThenCorrectStringIsReturned) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.LogApiCalls.set(true);
    MockBuffer buffer;
    MemObj *memoryObject = &buffer;
    cl_mem clMem = memoryObject;
    cl_mem clMemObjects[] = {clMem, clMem};
    cl_uint numObjects = 2;

    string memObjectString = debugManager.getMemObjects(reinterpret_cast<const uintptr_t *>(clMemObjects), numObjects);
    EXPECT_NE(0u, memObjectString.size());
    stringstream output;
    output << "cl_mem " << clMem << ", MemObj " << memoryObject;
    EXPECT_THAT(memObjectString, ::testing::HasSubstr(output.str()));
}

TEST(DebugSettingsManager, GivenDebugManagerWithDebugFunctionalityWhenGetMemObjectsIsCalledWithNullptrThenStringIsEmpty) {
    FullyEnabledTestDebugManager debugManager;
    string memObjectString = debugManager.getMemObjects(nullptr, 2);
    EXPECT_EQ(0u, memObjectString.size());
}

TEST(DebugSettingsManager, GivenDebugManagerWithoutDebugFunctionalityWhenGetMemObjectsIsCalledThenCallReturnsImmediately) {
    FullyDisabledTestDebugManager debugManager;
    string memObjectString = debugManager.getMemObjects(nullptr, 2);
    EXPECT_EQ(0u, memObjectString.size());
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernel) {
    FullyEnabledTestDebugManager debugManager;
    string kernelDumpFile = "testDumpKernel";

    // test kernel dumping
    debugManager.flags.DumpKernels.set(true);
    debugManager.dumpKernel(kernelDumpFile, "kernel source here");
    debugManager.flags.DumpKernels.set(false);
    EXPECT_TRUE(debugManager.wasFileCreated(kernelDumpFile.append(".txt")));
}

TEST(DebugSettingsManager, WithoutDebugFunctionalityDumpKernel) {
    FullyEnabledTestDebugManager debugManager;
    string kernelDumpFile = "testDumpKernel";

    // test kernel dumping
    debugManager.flags.DumpKernels.set(false);
    debugManager.dumpKernel(kernelDumpFile, "kernel source here");
    debugManager.flags.DumpKernels.set(false);
    EXPECT_FALSE(debugManager.wasFileCreated(kernelDumpFile.append(".txt")));
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpBinary) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernels.set(true);
    string programDumpFile = "programBinary.bin";
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    debugManager.dumpBinaryProgram(1, &length, &ptrBinary);
    debugManager.flags.DumpKernels.set(false);

    EXPECT_TRUE(debugManager.wasFileCreated(programDumpFile));
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpNullBinary) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernels.set(true);

    string programDumpFile = "programBinary.bin";
    size_t length = 4;
    debugManager.dumpBinaryProgram(1, &length, nullptr);
    debugManager.flags.DumpKernels.set(false);

    EXPECT_FALSE(debugManager.wasFileCreated(programDumpFile));
}

TEST(DebugSettingsManager, WithDebugFunctionalityDontDumpKernelsForNullMdi) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernels.set(true);
    debugManager.dumpKernelArgs((const MultiDispatchInfo *)nullptr);
    debugManager.flags.DumpKernels.set(false);

    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDontDumpKernelArgsForNullMdi) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs((const MultiDispatchInfo *)nullptr);
    debugManager.flags.DumpKernelArgs.set(false);

    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelArgsForMdi) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
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

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(multiDispatchInfo.get());
    debugManager.flags.DumpKernelArgs.set(false);

    // check if file was created
    std::string expectedFile = "_arg_0_immediate_size_32_flags_0.bin";
    EXPECT_TRUE(debugManager.wasFileCreated(expectedFile));

    // no files should be created for local buffer
    EXPECT_EQ(debugManager.createdFilesCount(), 1);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelNullKernel) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernels.set(true);
    debugManager.dumpKernelArgs((const Kernel *)nullptr);
    debugManager.flags.DumpKernels.set(false);

    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityAndEmptyKernelDontDumpKernelArgs) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    auto kernel = unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(kernel.get());
    debugManager.flags.DumpKernelArgs.set(false);

    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelArgsImmediate) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
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

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(kernel.get());
    debugManager.flags.DumpKernelArgs.set(false);

    // check if file was created
    EXPECT_TRUE(debugManager.wasFileCreated("_arg_0_immediate_size_32_flags_0.bin"));

    // no files should be created for local buffer
    EXPECT_EQ(debugManager.createdFilesCount(), 1);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelArgsImmediateZeroSize) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
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

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(kernel.get());
    debugManager.flags.DumpKernelArgs.set(false);

    // no files should be created for zero size
    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelArgsLocalBuffer) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto device = unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    auto kernel = unique_ptr<MockKernel>(new MockKernel(&program, *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].addressQualifier = static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_LOCAL);

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(kernel.get());
    debugManager.flags.DumpKernelArgs.set(false);

    // no files should be created for local buffer
    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelArgsBufferNotSet) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].typeStr = "uint8 *buffer";

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(kernel.get());
    debugManager.flags.DumpKernelArgs.set(false);

    // no files should be created for local buffer
    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelArgsBuffer) {
    unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto buffer = BufferHelper<>::create(context.get());
    cl_mem clObj = buffer;

    auto kernelInfo = std::make_unique<KernelInfo>();
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].typeStr = "uint8 *buffer";

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    kernel->setArg(0, clObj);

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(kernel.get());
    debugManager.flags.DumpKernelArgs.set(false);

    buffer->release();

    // check if file was created
    EXPECT_TRUE(debugManager.wasFileCreated("_arg_0_buffer_size_16_flags_1.bin"));

    // no files should be created for local buffer
    EXPECT_EQ(debugManager.createdFilesCount(), 1);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelArgsSampler) {
    auto kernelInfo = std::make_unique<KernelInfo>();
    unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *kernelInfo, *device));

    KernelArgPatchInfo kernelArgPatchInfo;

    kernelInfo->kernelArgInfo.resize(1);
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].typeStr = "sampler test";

    kernel->initialize();

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(kernel.get());
    debugManager.flags.DumpKernelArgs.set(false);

    // no files should be created for sampler arg
    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpKernelArgsImageNotSet) {

    auto kernelInfo = std::make_unique<KernelInfo>();
    unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<Device>(nullptr));
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
    kernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    kernelInfo->kernelArgInfo[0].typeStr = "image2d buffer";
    kernelInfo->kernelArgInfo[0].isImage = true;
    kernelInfo->kernelArgInfo[0].offsetImgWidth = 0x4;

    kernel->initialize();

    size_t crossThreadDataSize = sizeof(void *);
    auto crossThreadData = unique_ptr<uint8_t>(new uint8_t[crossThreadDataSize]);
    kernel->setCrossThreadData(crossThreadData.get(), static_cast<uint32_t>(crossThreadDataSize));

    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs(kernel.get());
    debugManager.flags.DumpKernelArgs.set(false);

    // no files should be created for local buffer
    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithDebugFunctionalityDumpBinaryNegativeCases) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.DumpKernels.set(true);
    size_t length = 1;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    debugManager.dumpBinaryProgram(1, nullptr, nullptr);
    debugManager.dumpBinaryProgram(1, nullptr, &ptrBinary);
    debugManager.dumpBinaryProgram(1, &length, nullptr);
    length = 0;
    debugManager.dumpBinaryProgram(1, &length, &ptrBinary);
    debugManager.dumpBinaryProgram(1, &length, nullptr);

    debugManager.flags.DumpKernels.set(false);

    EXPECT_EQ(debugManager.createdFilesCount(), 0);
}

TEST(DebugSettingsManager, WithoutDebugFunctionality) {
    string path = ".";
    vector<string> files = Directory::getFiles(path);
    size_t initialNumberOfFiles = files.size();

    FullyDisabledTestDebugManager debugManager;

    // Should not be enabled without debug functionality
    EXPECT_TRUE(debugManager.disabled());
    // SettingsReader not created
    EXPECT_EQ(nullptr, debugManager.getSettingsReader());

    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

// debug variables / flags set to default
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)                           \
    {                                                                                                       \
        bool isEqual = TestDebugFlagsChecker::isEqual(debugManager.flags.variableName.get(), defaultValue); \
        EXPECT_TRUE(isEqual);                                                                               \
    }
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE

    // test kernel dumping
    bool kernelDumpCreated = false;
    string kernelDumpFile = "testDumpKernel";
    debugManager.flags.DumpKernels.set(true);
    debugManager.dumpKernel(kernelDumpFile, "kernel source here");
    debugManager.flags.DumpKernels.set(false);
    kernelDumpCreated = fileExists(kernelDumpFile.append(".txt"));
    EXPECT_FALSE(kernelDumpCreated);

    // test api logging
    debugManager.flags.LogApiCalls.set(true);
    debugManager.logApiCall(__FUNCTION__, true, 0);
    debugManager.flags.LogApiCalls.set(false);
    logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // getInput returns 0
    size_t input = 5;
    size_t output = debugManager.getInput(&input, 0);
    EXPECT_EQ(0u, output);

    // getEvents returns 0-size string
    string event = debugManager.getEvents(&input, 0);
    EXPECT_EQ(0u, event.size());

    // getSizes returns 0-size string
    string lwsSizes = debugManager.getSizes(&input, 0, true);
    string gwsSizes = debugManager.getSizes(&input, 0, false);
    EXPECT_EQ(0u, lwsSizes.size());
    EXPECT_EQ(0u, gwsSizes.size());

    // no programDump file
    debugManager.flags.DumpKernels.set(true);
    string programDumpFile = "programBinary.bin";
    size_t length = 4;
    unsigned char binary[4];
    const unsigned char *ptrBinary = binary;
    debugManager.dumpBinaryProgram(1, &length, &ptrBinary);
    debugManager.flags.DumpKernels.set(false);
    EXPECT_FALSE(debugManager.wasFileCreated(programDumpFile));

    debugManager.flags.DumpKernelArgs.set(true);
    debugManager.dumpKernelArgs((const Kernel *)nullptr);
    debugManager.flags.DumpKernelArgs.set(false);

    // test api input logging
    debugManager.flags.LogApiCalls.set(true);
    debugManager.logInputs("Arg name", "value");
    debugManager.logInputs("int", 5);
    debugManager.flags.LogApiCalls.set(false);
    logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    // check Log
    debugManager.flags.LogApiCalls.set(true);
    debugManager.log(true, "string to be logged");
    debugManager.flags.LogApiCalls.set(false);
    logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    files = Directory::getFiles(path);
    size_t finalNumberOfFiles = files.size();

    EXPECT_EQ(initialNumberOfFiles, finalNumberOfFiles);
}

TEST(DebugSettingsApiEnterWrapper, WithDebugFunctionality) {

    const char *name = "function";
    int error = 0;
    TestDebugSettingsApiEnterWrapper<true> *debugApiWrapper = new TestDebugSettingsApiEnterWrapper<true>(name, nullptr);

    EXPECT_TRUE(debugApiWrapper->loggedEnter);

    delete debugApiWrapper;

    TestDebugSettingsApiEnterWrapper<true> *debugApiWrapper2 = new TestDebugSettingsApiEnterWrapper<true>(name, &error);

    EXPECT_TRUE(debugApiWrapper2->loggedEnter);

    delete debugApiWrapper2;
}

TEST(DebugSettingsApiEnterWrapper, WithoutDebugFunctionality) {

    const char *name = "function";
    int error = 0;
    TestDebugSettingsApiEnterWrapper<false> *debugApiWrapper = new TestDebugSettingsApiEnterWrapper<false>(name, nullptr);

    EXPECT_FALSE(debugApiWrapper->loggedEnter);

    delete debugApiWrapper;

    TestDebugSettingsApiEnterWrapper<false> *debugApiWrapper2 = new TestDebugSettingsApiEnterWrapper<false>(name, &error);

    EXPECT_FALSE(debugApiWrapper2->loggedEnter);

    delete debugApiWrapper2;
}

TEST(DebugSettingsManager, infoPointerToStringReturnsCorrectString) {
    FullyEnabledTestDebugManager debugManager;

    uint64_t value64bit = 64;
    string string64bit = debugManager.infoPointerToString(&value64bit, sizeof(uint64_t));
    uint32_t value32bit = 32;
    string string32bit = debugManager.infoPointerToString(&value32bit, sizeof(uint32_t));
    uint8_t value8bit = 0;
    string string8bit = debugManager.infoPointerToString(&value8bit, sizeof(uint8_t));

    EXPECT_STREQ("64", string64bit.c_str());
    EXPECT_STREQ("32", string32bit.c_str());
    EXPECT_STREQ("0", string8bit.c_str());

    string stringNonValue = debugManager.infoPointerToString(nullptr, 56);
    EXPECT_STREQ("", stringNonValue.c_str());

    char valueChar = 0;
    stringNonValue = debugManager.infoPointerToString(&valueChar, 56);
    EXPECT_STREQ("", stringNonValue.c_str());
}

TEST(DebugSettingsManager, givenDisabledDebugFunctionalityWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsDropped) {
    FullyDisabledTestDebugManager debugManager;
    bool wasCalled = false;
    debugManager.logLazyEvaluateArgs(true, [&] { wasCalled = true; });
    EXPECT_FALSE(wasCalled);
}

TEST(DebugSettingsManager, givenDisabledPredicateWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsDropped) {
    FullyEnabledTestDebugManager debugManager;
    bool wasCalled = false;
    debugManager.logLazyEvaluateArgs(false, [&] { wasCalled = true; });
    EXPECT_FALSE(wasCalled);
}

TEST(DebugSettingsManager, givenEnabledPredicateWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsExecuted) {
    FullyEnabledTestDebugManager debugManager;
    bool wasCalled = false;
    debugManager.logLazyEvaluateArgs(true, [&] { wasCalled = true; });
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

TEST(DebugSettingsManager, givenDisabledPredicateWhenDbgLogLazyEvaluateArgsIsCalledThenInputParametersAreNotEvaluated) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.LogApiCalls.set(false);

    bool wasCalled = false;
    DBG_LOG_LAZY_EVALUATE_ARGS(debugManager, LogApiCalls, log, true, DummyEvaluator(wasCalled));
    EXPECT_FALSE(wasCalled);
}

TEST(DebugSettingsManager, givenEnabledPredicateWhenDbgLogLazyEvaluateArgsIsCalledThenInputParametersAreEvaluated) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.LogApiCalls.set(true);

    bool wasCalled = false;
    DBG_LOG_LAZY_EVALUATE_ARGS(debugManager, LogApiCalls, log, true, DummyEvaluator(wasCalled));
    EXPECT_TRUE(wasCalled);
}

TEST(DebugSettingsManager, whenDebugManagerIsDisabledThenDebugFunctionalityIsNotAvailableAtCompileTime) {
    TestDebugSettingsManager<DebugFunctionalityLevel::None> debugManager;

    static_assert(debugManager.disabled(), "");
    static_assert(false == debugManager.debugLoggingAvailable(), "");
    static_assert(false == debugManager.debugKernelDumpingAvailable(), "");
    static_assert(false == debugManager.kernelArgDumpingAvailable(), "");
    static_assert(false == debugManager.registryReadAvailable(), "");
}

TEST(DebugSettingsManager, whenDebugManagerIsFullyEnabledThenAllDebugFunctionalityIsAvailableAtCompileTime) {
    TestDebugSettingsManager<DebugFunctionalityLevel::Full> debugManager;

    static_assert(false == debugManager.disabled(), "");
    static_assert(debugManager.debugLoggingAvailable(), "");
    static_assert(debugManager.debugKernelDumpingAvailable(), "");
    static_assert(debugManager.kernelArgDumpingAvailable(), "");
    static_assert(debugManager.registryReadAvailable(), "");
}

TEST(DebugSettingsManager, whenOnlyRegKeysAreEnabledThenAllOtherDebugFunctionalityIsNotAvailableAtCompileTime) {
    TestDebugSettingsManager<DebugFunctionalityLevel::RegKeys> debugManager;

    static_assert(false == debugManager.disabled(), "");
    static_assert(false == debugManager.debugLoggingAvailable(), "");
    static_assert(false == debugManager.debugKernelDumpingAvailable(), "");
    static_assert(false == debugManager.kernelArgDumpingAvailable(), "");
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
    auto readerImpl = SettingsReader::create();
    debugManager.setReaderImpl(readerImpl);
    EXPECT_EQ(readerImpl, debugManager.getReaderImpl());
}
TEST(DebugSettingsManager, givenReaderImplInDebugManagerWhenSettingDifferentReaderImplThenItsSetProperly) {
    FullyDisabledTestDebugManager debugManager;
    auto readerImpl = SettingsReader::create();
    debugManager.setReaderImpl(readerImpl);

    auto readerImpl2 = SettingsReader::create();
    debugManager.setReaderImpl(readerImpl2);
    EXPECT_EQ(readerImpl2, debugManager.getReaderImpl());
}

TEST(DebugSettingsManager, givenPrintDebugSettingsEnabledWhenCallingDumpFlagsThenFlagsAreWrittenToDumpFile) {
    FullyEnabledTestDebugManager debugManager;
    debugManager.flags.PrintDebugSettings.set(true);
    debugManager.flags.LoopAtPlatformInitialize.set(true);
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
    {GraphicsAllocation::AllocationType::UNKNOWN, "UNKNOWN"}};

class AllocationTypeLogging : public ::testing::TestWithParam<AllocationTypeTestCase> {};

TEST_P(AllocationTypeLogging, givenGraphicsAllocationTypeWhenConvertingToStringThenCorrectStringIsReturned) {
    FullyEnabledTestDebugManager debugManager;
    auto input = GetParam();

    GraphicsAllocation graphicsAllocation(input.type, nullptr, 0u, 0, MemoryPool::MemoryNull, true);

    auto result = debugManager.getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, input.str);
}

INSTANTIATE_TEST_CASE_P(AllAllocationTypes,
                        AllocationTypeLogging,
                        ::testing::ValuesIn(allocationTypeValues));

TEST(AllocationTypeLoggingSingle, givenGraphicsAllocationTypeWhenConvertingToStringIllegalValueThenILLEGAL_VALUEIsReturned) {
    FullyEnabledTestDebugManager debugManager;

    GraphicsAllocation graphicsAllocation(static_cast<GraphicsAllocation::AllocationType>(999), nullptr, 0u, 0, MemoryPool::MemoryNull, true);

    auto result = debugManager.getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, "ILLEGAL_VALUE");
}

TEST(AllocationTypeLoggingSingle, givenGraphicsAllocationTypeWhenDebugManagerDisabledThennullptrReturned) {
    FullyDisabledTestDebugManager debugManager;

    GraphicsAllocation graphicsAllocation(GraphicsAllocation::AllocationType::BUFFER, nullptr, 0u, 0, MemoryPool::MemoryNull, true);

    auto result = debugManager.getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, nullptr);
}

TEST(AllocationInfoLogging, givenBaseGraphicsAllocationWhenGettingImplementationSpecificAllocationInfoThenReturnEmptyInfoString) {
    GraphicsAllocation graphicsAllocation(GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0u, 0, MemoryPool::MemoryNull, true);
    EXPECT_STREQ(graphicsAllocation.getAllocationInfoString().c_str(), "");
}
