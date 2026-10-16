/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/utilities/logger_tests.h"

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/utilities/kernel_dispatch_stats.h"
#include "shared/source/utilities/logger.h"
#include "shared/source/utilities/logger_neo_only.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_io_functions.h"
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
    FullyEnabledFileLogger fileLogger(std::string(""));

    EXPECT_TRUE(fileLogger.enabled());
}

TEST(FileLogger, GivenReleseInternalFileLoggerIsCreatedThenItIsEnabled) {
    ReleaseInternalFileLogger fileLogger(std::string(""));

    EXPECT_TRUE(fileLogger.enabled());
}

TEST(FileLogger, GivenFullyDisabledFileLoggerIsCreatedThenItIsDisabled) {
    FullyDisabledFileLogger fileLogger(std::string(""));

    EXPECT_FALSE(fileLogger.enabled());
}

TEST(FileLogger, GivenFileLoggerWhenSettingFileNameThenCorrectFilenameIsSet) {
    FullyEnabledFileLogger fileLogger(std::string(""));
    fileLogger.setLogFileName("new_filename");
    EXPECT_STREQ("new_filename", fileLogger.getLogFileName());
}

TEST(FileLogger, GivenIncorrectFilenameFileWhenLoggingApiCallsThenFileIsNotCreated) {
    FullyEnabledFileLogger fileLogger(std::string("test.log"));
    fileLogger.writeToFile("", "", 0, std::ios_base::in | std::ios_base::out);

    EXPECT_FALSE(fileLogger.wasFileCreated(fileLogger.getLogFileName()));
    EXPECT_FALSE(virtualFileExists(fileLogger.getLogFileName()));
}

TEST(FileLogger, GivenCorrectFilenameFileWhenLoggingApiCallsThenFileIsCreated) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);
    fileLogger.writeToFile(testFile, "test", 4, std::fstream::out);

    EXPECT_TRUE(virtualFileExists(fileLogger.getLogFileName()));
    if (virtualFileExists(fileLogger.getLogFileName())) {
        removeVirtualFile(fileLogger.getLogFileName());
    }
}

TEST(FileLogger, GivenSameFileNameWhenCreatingNewInstanceThenOldFileIsRemoved) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);
    fileLogger.writeToFile(fileLogger.getLogFileName(), "test", 4, std::fstream::out);

    EXPECT_TRUE(virtualFileExists(fileLogger.getLogFileName()));
    FullyEnabledFileLogger fileLogger2(testFile);
    EXPECT_FALSE(virtualFileExists(fileLogger.getLogFileName()));
}

TEST(FileLogger, GivenSameFileNameWhenCreatingNewFullyDisabledLoggerThenOldFileIsNotRemoved) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);
    fileLogger.writeToFile(fileLogger.getLogFileName(), "test", 4, std::fstream::out);

    EXPECT_TRUE(virtualFileExists(fileLogger.getLogFileName()));
    FullyDisabledFileLogger fileLogger2(testFile);
    EXPECT_TRUE(virtualFileExists(fileLogger.getLogFileName()));
    if (virtualFileExists(fileLogger.getLogFileName())) {
        removeVirtualFile(fileLogger.getLogFileName());
    }
}

TEST(FileLogger, givenDisabledDebugFunctionalityWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsDropped) {
    std::string testFile = "testfile";
    FullyDisabledFileLogger fileLogger(testFile);

    bool wasCalled = false;
    fileLogger.logLazyEvaluateArgs(true, [&] { wasCalled = true; });
    EXPECT_FALSE(wasCalled);
}

TEST(FileLogger, givenDisabledPredicateWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsDropped) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);

    bool wasCalled = false;
    fileLogger.logLazyEvaluateArgs(false, [&] { wasCalled = true; });
    EXPECT_FALSE(wasCalled);
}

TEST(FileLogger, givenEnabledPredicateWhenLogLazyEvaluateArgsIsCalledThenCallToLambdaIsExecuted) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);
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
    FullyEnabledFileLogger fileLogger(testFile);

    bool wasCalled = false;
    DBG_LOG_LAZY_EVALUATE_ARGS(fileLogger, false, log, true, DummyEvaluator(wasCalled));
    EXPECT_FALSE(wasCalled);
}

TEST(FileLogger, givenEnabledPredicateWhenDbgLogLazyEvaluateArgsIsCalledThenInputParametersAreEvaluated) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);

    bool wasCalled = false;
    DBG_LOG_LAZY_EVALUATE_ARGS(fileLogger, true, log, true, DummyEvaluator(wasCalled));
    EXPECT_TRUE(wasCalled);
}

TEST(FileLogger, whenDisabledThenDebugFunctionalityIsNotAvailableAtCompileTime) {
    std::string testFile = "testfile";
    FileLogger<DebugFunctionalityLevel::none> fileLogger(testFile);

    static_assert(false == fileLogger.enabled(), "");
}

TEST(FileLogger, whenFullyEnabledThenAllDebugFunctionalityIsAvailableAtCompileTime) {
    std::string testFile = "testfile";
    FileLogger<DebugFunctionalityLevel::full> fileLogger(testFile);

    static_assert(true == fileLogger.enabled(), "");
}

TEST(FileLogger, givenEnabledLogWhenLogDebugStringCalledThenStringIsWrittenToFile) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);

    fileLogger.logDebugString(true, "test log");
    EXPECT_EQ(std::string("test log"), fileLogger.getFileString(testFile));
}

TEST(FileLogger, givenDisabledLogWhenLogDebugStringCalledThenStringIsNotWrittenToFile) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);

    fileLogger.logDebugString(false, "test log");
    EXPECT_EQ(0u, fileLogger.getFileString(testFile).size());
}

TEST(FileLogger, givenDisabledFileLoggerWhenLogDebugStringCalledThenFileIsNotCreated) {
    std::string testFile = "testfile";
    FullyDisabledFileLogger fileLogger(testFile);

    fileLogger.logDebugString(true, "test log");
    EXPECT_FALSE(fileLogger.wasFileCreated(testFile));
}

TEST(FileLogger, GivenFileLoggerInstanceWhenCreatedThenFileNameContainsProcessId) {
    auto &logger = NEO::fileLoggerInstance();
    auto logFileName = logger.getLogFileName();

    auto actualPid = SysCalls::getProcessId();
    std::stringstream expectedSubstring;
    expectedSubstring << "igdrcl_" << actualPid << ".log";

    EXPECT_STREQ(expectedSubstring.str().c_str(), logFileName);
}

TEST(FileLogger, GivenFileLoggerInstanceWhenCalledMultipleTimesThenReturnsSameSingleton) {
    auto &logger1 = NEO::fileLoggerInstance();
    auto &logger2 = NEO::fileLoggerInstance();

    EXPECT_EQ(&logger1, &logger2);

    std::string name1 = logger1.getLogFileName();
    std::string name2 = logger2.getLogFileName();

    EXPECT_STREQ(name1.c_str(), name2.c_str());
}

TEST(FileLogger, givenNoFileNameOverrideWhenQueryingFileLoggerFileNameThenPidBasedNameIsReturned) {
    DebugVariables flags;

    auto logFileName = NEO::getFileLoggerFileName(flags);

    auto actualPid = SysCalls::getProcessId();
    std::stringstream expectedFileName;
    expectedFileName << "igdrcl_" << actualPid << ".log";

    EXPECT_STREQ(expectedFileName.str().c_str(), logFileName.c_str());
}

TEST(FileLogger, givenFileNameOverrideWhenQueryingFileLoggerFileNameThenOverriddenNameIsReturned) {
    DebugVariables flags;
    flags.OverrideIgdrclLogFileName.set("forced_igdrcl.log");

    auto logFileName = NEO::getFileLoggerFileName(flags);

    EXPECT_STREQ("forced_igdrcl.log", logFileName.c_str());
}

TEST(AllocationTypeLogging, givenGraphicsAllocationTypeWhenConvertingToStringThenCorrectStringIsReturned) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);

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
         {AllocationType::swTagBuffer, "SW_TAG_BUFFER"},
         {AllocationType::hostFunction, "HOST_FUNCTION"}}};

    for (const auto &[type, str] : allocationTypeValues) {
        GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, type, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);
        auto result = getAllocationTypeString(&graphicsAllocation);

        EXPECT_STREQ(result, str);
    }
}

TEST(AllocationTypeLoggingSingle, givenGraphicsAllocationTypeWhenConvertingToStringIllegalValueThenILLEGAL_VALUEIsReturned) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);

    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, static_cast<AllocationType>(999), nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)

    auto result = getAllocationTypeString(&graphicsAllocation);

    EXPECT_STREQ(result, "ILLEGAL_VALUE");
}

TEST(AllocationTypeLoggingSingle, givenAllocationTypeWhenConvertingToStringThenSupportAll) {
    std::string testFile = "testfile";
    FullyEnabledFileLogger fileLogger(testFile);

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

    FullyEnabledAllocationLogger fileLogger(testFile, flags);

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

    FullyEnabledAllocationLogger fileLogger(testFile, flags);

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
    FullyEnabledFileLogger fileLogger(testFile);

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

TEST(CpuGpuVaLogging, givenAllocationWithCpuVaWhenLoggingAllocationThenCpuVaRangeIsLoggedToFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(1);

    FullyEnabledAllocationLogger fileLogger(testFile, flags);

    char buffer[4096];
    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::buffer, buffer, reinterpret_cast<uint64_t>(buffer), 0, MemoryPool::system4KBPages, MemoryManager::maxOsContextCount, sizeof(buffer));

    logAllocation(fileLogger, &graphicsAllocation, nullptr);

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find("CPU VA: 0x") != std::string::npos);
        EXPECT_FALSE(str.find("CPU VA: NULL") != std::string::npos);
        EXPECT_TRUE(str.find(" - 0x") != std::string::npos);
    } else {
        EXPECT_FALSE(true);
    }
}

TEST(CpuGpuVaLogging, givenAllocationWithoutCpuVaWhenLoggingAllocationThenNullIsLoggedToFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(1);

    FullyEnabledAllocationLogger fileLogger(testFile, flags);

    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::buffer, nullptr, 0x1000, 0, MemoryPool::localMemory, MemoryManager::maxOsContextCount, 4096);

    logAllocation(fileLogger, &graphicsAllocation, nullptr);

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find("CPU VA: NULL") != std::string::npos);
    } else {
        EXPECT_FALSE(true);
    }
}

TEST(PrintsRedirection, GivenForcePrintsRedirectionWhenStdoutForcedThenItWorksCorrectly) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForcePrintsRedirection.set(1);

    int i = 4;
    int j = 5;
    StreamCapture capture;
    capture.captureStdout();
    PRINT_STRING(true, stdout, "same as default: %d, ", i);
    PRINT_STRING(true, stderr, "forced: %d", j);
    auto output = capture.getCapturedStdout();
    EXPECT_STREQ(output.c_str(), "same as default: 4, forced: 5");
}

TEST(PrintsRedirection, GivenForcePrintsRedirectionWhenStderrForcedThenItWorksCorrectly) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForcePrintsRedirection.set(2);

    int i = 4;
    int j = 5;
    StreamCapture capture;
    capture.captureStderr();
    PRINT_STRING(true, stdout, "forced: %d, ", i);
    PRINT_STRING(true, stderr, "same as default: %d", j);
    auto output = capture.getCapturedStderr();
    EXPECT_STREQ(output.c_str(), "forced: 4, same as default: 5");
}

template <>
struct NEO::FileLoggerProxy<false> {

    static constexpr std::string_view logFileName = "mock_igdrcl.log";
    ::FullyEnabledFileLogger virtualFileLogger = {std::string(logFileName)};

    void logString(std::string_view data) {
        this->virtualFileLogger.logDebugString(true, data);
    }

    auto getCapturedString() {
        return this->virtualFileLogger.getFileString(std::string(logFileName));
    }
};

TEST(PrintsRedirection, GivenForcePrintsRedirectionWhenFileOutputForcedThenItWorksCorrectly) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForcePrintsRedirection.set(3);

    int i = 4;
    StreamCapture capture;
    capture.captureStderr();
    capture.captureStdout();

    auto callCnt = NEO::IoFunctions::mockVsnprintfCalled;
    auto loggerProxy = NEO::FileLoggerProxy<false>{};
    NEO::printStringForMacroUseOnly(loggerProxy, stdout, "%s: %d\n", "redirected", i);
    EXPECT_EQ(NEO::IoFunctions::mockVsnprintfCalled, callCnt + 1U);
    auto capturedFileOutput = loggerProxy.getCapturedString();
    auto capturedStdout = capture.getCapturedStdout();
    auto capturedStderr = capture.getCapturedStderr();
    EXPECT_STREQ(capturedStdout.c_str(), "");
    EXPECT_STREQ(capturedStderr.c_str(), "");
    EXPECT_STREQ(capturedFileOutput.c_str(), "redirected: 4\n");
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenSysAndLocalAllocationsTrackedThenFullReportPrinted) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        fileLogger.trackAllocationForSummary("BUFFER", 1024, false);
        fileLogger.trackAllocationForSummary("LINEAR_STREAM", 512, false);
        fileLogger.trackAllocationForSummary("KERNEL_ISA", 2048, true);
        fileLogger.trackLiveAllocation("BUFFER", 1024, false);
        fileLogger.trackLiveAllocation("KERNEL_ISA", 2048, true);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Allocation Summary Report") != std::string::npos);
    EXPECT_TRUE(output.find("System Memory Allocations") != std::string::npos);
    EXPECT_TRUE(output.find("Local Memory Allocations") != std::string::npos);
    EXPECT_TRUE(output.find("BUFFER") != std::string::npos);
    EXPECT_TRUE(output.find("KERNEL_ISA") != std::string::npos);
    EXPECT_TRUE(output.find("Peak Live Memory") != std::string::npos);
    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("Peak Local Memory") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenOnlyLocalLiveAllocationsTrackedThenPeakLocalConditionEvaluated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        fileLogger.trackAllocationForSummary("KERNEL_ISA", 2048, true);
        fileLogger.trackLiveAllocation("KERNEL_ISA", 2048, true);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Live Memory") != std::string::npos);
    EXPECT_TRUE(output.find("Peak Local Memory") != std::string::npos);
    EXPECT_FALSE(output.find("Peak System Memory") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenUntrackExceedsTrackedThenSysMemClampedToZero) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        fileLogger.trackAllocationForSummary("BUFFER", 100, false);
        fileLogger.trackLiveAllocation("BUFFER", 100, false);
        fileLogger.untrackLiveAllocation("BUFFER", 50, false);
        fileLogger.untrackLiveAllocation("BUFFER", 200, false);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("100") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenNoLiveAllocationsTrackedThenNoPeakSectionPrinted) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        fileLogger.trackAllocationForSummary("BUFFER", 1024, false);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Allocation Summary Report") != std::string::npos);
    EXPECT_FALSE(output.find("Peak Live Memory") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenSysLiveAllocBelowPeakThenPeakNotUpdatedAndMissingTypeHandled) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        fileLogger.trackAllocationForSummary("BUFFER", 100, false);
        fileLogger.trackLiveAllocation("BUFFER", 100, false);
        fileLogger.untrackLiveAllocation("BUFFER", 90, false);
        fileLogger.trackLiveAllocation("BUFFER", 50, false);
        fileLogger.untrackLiveAllocation("LINEAR_STREAM", 50, false);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("100 bytes") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenNoSummaryAllocationsTrackedThenEarlyReturnWithNoOutput) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        fileLogger.trackLiveAllocation("BUFFER", 100, false);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_FALSE(output.find("Allocation Summary Report") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenLocalLiveBelowPeakAndUnderflowThenClampedToZero) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        fileLogger.trackAllocationForSummary("KERNEL_ISA", 100, true);
        fileLogger.trackLiveAllocation("KERNEL_ISA", 100, true);
        fileLogger.untrackLiveAllocation("KERNEL_ISA", 200, true);
        fileLogger.trackLiveAllocation("KERNEL_ISA", 50, true);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Local Memory") != std::string::npos);
    EXPECT_TRUE(output.find("100 bytes") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenLocalUntrackWithinBudgetThenLocalMemoryDecremented) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        fileLogger.trackAllocationForSummary("KERNEL_ISA", 200, true);
        fileLogger.trackLiveAllocation("KERNEL_ISA", 200, true);
        fileLogger.untrackLiveAllocation("KERNEL_ISA", 50, true);
        fileLogger.untrackLiveAllocation("BUFFER", 50, true);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Local Memory") != std::string::npos);
    EXPECT_TRUE(output.find("200 bytes") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenFreeAllocationCalledWithReportEnabledThenLiveTrackingUpdated) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    GraphicsAllocation sysAllocation(0u, 1u, AllocationType::buffer, nullptr, uint64_t{0}, uint64_t{0}, size_t{1024}, MemoryPool::system64KBPages, MemoryManager::maxOsContextCount);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledAllocationLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &sysAllocation, nullptr);
        logFreeAllocation(fileLogger, &sysAllocation);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("BUFFER") != std::string::npos);
}

TEST(FileLogger, givenKernelDispatchStatsEnabledWhenUsmReusePerfLoggerIsDestroyedThenReportFileIsNotWritten) {
    DebugManagerStateRestore restorer;
    debugManager.flags.LogKernelDispatchStats.set(true);
    removeVirtualFile(getKernelDispatchStatsFileName());

    {
        UsmReusePerfLogger usmReuseLogger;
    }

    EXPECT_FALSE(virtualFileExists(getKernelDispatchStatsFileName()));
}
