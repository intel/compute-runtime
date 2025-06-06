/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/utilities/logger_tests.h"

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/utilities/logger.h"
#include "shared/source/utilities/logger_neo_only.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "gtest/gtest.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

using namespace NEO;

TEST(FileLogger, GivenFullyEnabledFileLoggerIsCreatedThenItIsEnabled) {
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(std::string(""), flags);

    EXPECT_TRUE(fileLogger.enabled());
}

TEST(FileLogger, GivenReleseInternalFileLoggerIsCreatedThenItIsEnabled) {
    DebugVariables flags;
    ReleaseInternalFileLogger fileLogger(std::string(""), flags);

    EXPECT_TRUE(fileLogger.enabled());
}

TEST(FileLogger, GivenFullyDisabledFileLoggerIsCreatedThenItIsDisabled) {
    DebugVariables flags;
    FullyDisabledFileLogger fileLogger(std::string(""), flags);

    EXPECT_FALSE(fileLogger.enabled());
}

TEST(FileLogger, GivenFileLoggerWhenSettingFileNameThenCorrectFilenameIsSet) {
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(std::string(""), flags);
    fileLogger.useRealFiles(false);
    fileLogger.setLogFileName("new_filename");
    EXPECT_STREQ("new_filename", fileLogger.getLogFileName());
}

TEST(FileLogger, GivenEnabledDebugFunctinalityWhenLoggingApiCallsThenDumpToFile) {
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(std::string("test.log"), flags);
    fileLogger.useRealFiles(false);

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
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
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
    fileLogger.writeToFile("", "", 0, std::ios_base::in | std::ios_base::out);

    EXPECT_FALSE(fileLogger.wasFileCreated(fileLogger.getLogFileName()));
    EXPECT_FALSE(virtualFileExists(fileLogger.getLogFileName()));
}

TEST(FileLogger, GivenCorrectFilenameFileWhenLoggingApiCallsThenFileIsCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(false);
    fileLogger.writeToFile(testFile, "test", 4, std::fstream::out);

    EXPECT_TRUE(virtualFileExists(fileLogger.getLogFileName()));
    if (virtualFileExists(fileLogger.getLogFileName())) {
        removeVirtualFile(fileLogger.getLogFileName());
    }
}

TEST(FileLogger, GivenSameFileNameWhenCreatingNewInstanceThenOldFileIsRemoved) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(false);
    fileLogger.writeToFile(fileLogger.getLogFileName(), "test", 4, std::fstream::out);

    EXPECT_TRUE(virtualFileExists(fileLogger.getLogFileName()));
    FullyEnabledFileLogger fileLogger2(testFile, flags);
    EXPECT_FALSE(virtualFileExists(fileLogger.getLogFileName()));
}

TEST(FileLogger, GivenSameFileNameWhenCreatingNewFullyDisabledLoggerThenOldFileIsNotRemoved) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(false);
    fileLogger.writeToFile(fileLogger.getLogFileName(), "test", 4, std::fstream::out);

    EXPECT_TRUE(virtualFileExists(fileLogger.getLogFileName()));
    FullyDisabledFileLogger fileLogger2(testFile, flags);
    EXPECT_TRUE(virtualFileExists(fileLogger.getLogFileName()));
    if (virtualFileExists(fileLogger.getLogFileName())) {
        removeVirtualFile(fileLogger.getLogFileName());
    }
}

TEST(FileLogger, GivenFlagIsFalseWhenLoggingThenOnlyCustomLogsAreDumped) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogApiCalls.set(false);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(false);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
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

TEST(FileLogger, WhenDumpingKernelThenFileIsCreated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.DumpKernels.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(false);
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
    fileLogger.useRealFiles(false);
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

    operator const char *() const {
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
    FileLogger<DebugFunctionalityLevel::none> fileLogger(testFile, flags);

    static_assert(false == fileLogger.enabled(), "");
}

TEST(FileLogger, whenFullyEnabledThenAllDebugFunctionalityIsAvailableAtCompileTime) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FileLogger<DebugFunctionalityLevel::full> fileLogger(testFile, flags);

    static_assert(true == fileLogger.enabled(), "");
}

TEST(FileLogger, givenEnabledLogWhenLogDebugStringCalledThenStringIsWrittenToFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(false);

    fileLogger.logDebugString(true, "test log");
    EXPECT_EQ(std::string("test log"), fileLogger.getFileString(testFile));
}

TEST(FileLogger, givenDisabledLogWhenLogDebugStringCalledThenStringIsNotWrittenToFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    fileLogger.logDebugString(false, "test log");
    EXPECT_EQ(0u, fileLogger.getFileString(testFile).size());
}

TEST(AllocationTypeLogging, givenGraphicsAllocationTypeWhenConvertingToStringThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    std::array<std::pair<NEO::AllocationType, const char *>, 41> allocationTypeValues = {
        {{AllocationType::buffer, "BUFFER"},
         {AllocationType::bufferHostMemory, "BUFFER_HOST_MEMORY"},
         {AllocationType::commandBuffer, "COMMAND_BUFFER"},
         {AllocationType::constantSurface, "CONSTANT_SURFACE"},
         {AllocationType::externalHostPtr, "EXTERNAL_HOST_PTR"},
         {AllocationType::fillPattern, "FILL_PATTERN"},
         {AllocationType::globalSurface, "GLOBAL_SURFACE"},
         {AllocationType::image, "IMAGE"},
         {AllocationType::indirectObjectHeap, "INDIRECT_OBJECT_HEAP"},
         {AllocationType::instructionHeap, "INSTRUCTION_HEAP"},
         {AllocationType::internalHeap, "INTERNAL_HEAP"},
         {AllocationType::internalHostMemory, "INTERNAL_HOST_MEMORY"},
         {AllocationType::kernelIsa, "KERNEL_ISA"},
         {AllocationType::kernelIsaInternal, "KERNEL_ISA_INTERNAL"},
         {AllocationType::linearStream, "LINEAR_STREAM"},
         {AllocationType::mapAllocation, "MAP_ALLOCATION"},
         {AllocationType::mcs, "MCS"},
         {AllocationType::pipe, "PIPE"},
         {AllocationType::preemption, "PREEMPTION"},
         {AllocationType::printfSurface, "PRINTF_SURFACE"},
         {AllocationType::privateSurface, "PRIVATE_SURFACE"},
         {AllocationType::profilingTagBuffer, "PROFILING_TAG_BUFFER"},
         {AllocationType::scratchSurface, "SCRATCH_SURFACE"},
         {AllocationType::workPartitionSurface, "WORK_PARTITION_SURFACE"},
         {AllocationType::sharedBuffer, "SHARED_BUFFER"},
         {AllocationType::sharedImage, "SHARED_IMAGE"},
         {AllocationType::sharedResourceCopy, "SHARED_RESOURCE_COPY"},
         {AllocationType::surfaceStateHeap, "SURFACE_STATE_HEAP"},
         {AllocationType::svmCpu, "SVM_CPU"},
         {AllocationType::svmGpu, "SVM_GPU"},
         {AllocationType::svmZeroCopy, "SVM_ZERO_COPY"},
         {AllocationType::syncBuffer, "SYNC_BUFFER"},
         {AllocationType::tagBuffer, "TAG_BUFFER"},
         {AllocationType::globalFence, "GLOBAL_FENCE"},
         {AllocationType::timestampPacketTagBuffer, "TIMESTAMP_PACKET_TAG_BUFFER"},
         {AllocationType::unknown, "UNKNOWN"},
         {AllocationType::writeCombined, "WRITE_COMBINED"},
         {AllocationType::debugContextSaveArea, "DEBUG_CONTEXT_SAVE_AREA"},
         {AllocationType::debugSbaTrackingBuffer, "DEBUG_SBA_TRACKING_BUFFER"},
         {AllocationType::debugModuleArea, "DEBUG_MODULE_AREA"},
         {AllocationType::swTagBuffer, "SW_TAG_BUFFER"}}};

    for (const auto &[type, str] : allocationTypeValues) {
        GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, type, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);
        auto result = getAllocationTypeString(&graphicsAllocation);

        EXPECT_STREQ(result, str);
    }
}

TEST(AllocationTypeLoggingSingle, givenGraphicsAllocationTypeWhenConvertingToStringIllegalValueThenILLEGAL_VALUEIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, static_cast<AllocationType>(999), nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

    auto result = getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, "ILLEGAL_VALUE");
}

TEST(AllocationTypeLoggingSingle, givenAllocationTypeWhenConvertingToStringThenSupportAll) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);

    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
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

    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);

    StreamCapture capture;
    capture.captureStdout();
    logAllocation(fileLogger, &graphicsAllocation, nullptr);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput = "Created Graphics Allocation of type COMMAND_BUFFER\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.c_str());
}

TEST(AllocationTypeLoggingSingle, givenLogAllocationTypeWhenLoggingAllocationThenTypeIsLoggedToFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationType.set(1);

    FullyEnabledFileLogger fileLogger(testFile, flags);
    fileLogger.useRealFiles(false);

    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    StreamCapture capture;
    capture.captureStdout();
    logAllocation(fileLogger, &graphicsAllocation, nullptr);

    std::string output = capture.getCapturedStdout();
    std::string expectedOutput = "Created Graphics Allocation of type COMMAND_BUFFER\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.c_str());

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find("Type: ") != std::string::npos);
    } else {
        EXPECT_FALSE(true);
    }
}

TEST(MemoryPoolLogging, givenGraphicsMemoryPoolWhenConvertingToStringThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    DebugVariables flags;
    FullyEnabledFileLogger fileLogger(testFile, flags);

    std::array<std::pair<MemoryPool, const char *>, 7> memoryPoolValues = {
        {{MemoryPool::memoryNull, "MemoryNull"},
         {MemoryPool::localMemory, "LocalMemory"},
         {MemoryPool::system4KBPages, "System4KBPages"},
         {MemoryPool::system4KBPagesWith32BitGpuAddressing, "System4KBPagesWith32BitGpuAddressing"},
         {MemoryPool::system64KBPages, "System64KBPages"},
         {MemoryPool::system64KBPagesWith32BitGpuAddressing, "System64KBPagesWith32BitGpuAddressing"},
         {MemoryPool::systemCpuInaccessible, "SystemCpuInaccessible"}}};

    for (const auto &[pool, str] : memoryPoolValues) {
        GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, pool, MemoryManager::maxOsContextCount, 0llu);
        auto result = getMemoryPoolString(&graphicsAllocation);

        EXPECT_STREQ(result, str);
    }
}
