/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/utilities/logger_neo_only.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/logger_tests.h"

#include "gtest/gtest.h"

#include <ios>
#include <memory>
#include <ostream>
#include <sstream>
#include <thread>
#include <vector>

using namespace NEO;

using FileLoggerTests = Test<MockExecutionEnvironmentGmmFixture>;

TEST_F(FileLoggerTests, GivenLogAllocationMemoryPoolFlagThenLogsCorrectInfo) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->initializeMemoryManager();

    MockWddmAllocation allocation(getGmmHelper());
    allocation.handle = 4;
    allocation.setAllocationType(AllocationType::buffer);
    allocation.memoryPool = MemoryPool::system64KBPages;
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(allocation.getDefaultGmm()->resourceParamsData.data());
    gmmResourceParams->Flags.Info.NonLocalOnly = 0;
    gmmResourceParams->Usage = GMM_RESOURCE_USAGE_HEAP_STATELESS_DATA_PORT_L1_CACHED;
    allocation.setGpuAddress(0x12345);
    allocation.size = 777u;

    logAllocation(fileLogger, &allocation, executionEnvironment->memoryManager.get());

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " Pool: " << getMemoryPoolString(&allocation);

    std::stringstream gpuAddressCheck;
    gpuAddressCheck << " GPU VA: 0x" << std::hex << allocation.getGpuAddress();

    std::stringstream rootDeviceIndexCheck;
    rootDeviceIndexCheck << " Root index: " << allocation.getRootDeviceIndex();

    std::stringstream totalSystemMemoryCheck;
    totalSystemMemoryCheck << "Total sys mem allocated: " << std::dec << executionEnvironment->memoryManager->getUsedSystemMemorySize();

    std::stringstream totalLocalMemoryCheck;
    totalLocalMemoryCheck << "Total lmem allocated: " << std::dec << executionEnvironment->memoryManager->getUsedLocalMemorySize(0);

    ASSERT_TRUE(fileLogger.wasFileCreated(fileLogger.getLogFileName()));
    auto str = fileLogger.getFileString(fileLogger.getLogFileName());
    EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
    EXPECT_TRUE(str.find("Handle: 4") != std::string::npos);
    EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
    EXPECT_TRUE(str.find(gpuAddressCheck.str()) != std::string::npos);
    EXPECT_TRUE(str.find(rootDeviceIndexCheck.str()) != std::string::npos);
    EXPECT_TRUE(str.find("Type: BUFFER") != std::string::npos);
    EXPECT_TRUE(str.find("Size: 777") != std::string::npos);
    EXPECT_TRUE(str.find(totalSystemMemoryCheck.str()) != std::string::npos);
    EXPECT_TRUE(str.find(totalLocalMemoryCheck.str()) != std::string::npos);
}

TEST_F(FileLoggerTests, GivenLogAllocationMemoryPoolFlagSetFalseThenAllocationIsNotLogged) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(false);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.handle = 4;
    allocation.setAllocationType(AllocationType::buffer);
    allocation.memoryPool = MemoryPool::system64KBPages;
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(allocation.getDefaultGmm()->resourceParamsData.data());
    gmmResourceParams->Flags.Info.NonLocalOnly = 0;

    logAllocation(fileLogger, &allocation, nullptr);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << getMemoryPoolString(&allocation);

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_FALSE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("Handle: 4") != std::string::npos);
        EXPECT_FALSE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("AllocationType: BUFFER") != std::string::npos);
        EXPECT_FALSE(str.find("NonLocalOnly: 0") != std::string::npos);
    }
}

TEST(FileLogger, givenLogAllocationSummaryReportFlagWhenSysMemAllocationLoggedThenSummaryPrintedToStdoutOnDestruction) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setAllocationType(AllocationType::buffer);
    allocation.memoryPool = MemoryPool::system64KBPages;
    allocation.setSize(1024u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocation, nullptr);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Allocation Summary Report") != std::string::npos);
    EXPECT_TRUE(output.find("System Memory Allocations") != std::string::npos);
    EXPECT_TRUE(output.find("BUFFER") != std::string::npos);
    EXPECT_TRUE(output.find("1024") != std::string::npos);
    EXPECT_TRUE(output.find("100.00%") != std::string::npos);
    EXPECT_TRUE(output.find("Local Memory Allocations") == std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportFlagWhenLocalMemAllocationLoggedThenLocalSummaryPrintedOnDestruction) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setAllocationType(AllocationType::kernelIsa);
    allocation.memoryPool = MemoryPool::localMemory;
    allocation.setSize(2048u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocation, nullptr);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Allocation Summary Report") != std::string::npos);
    EXPECT_TRUE(output.find("Local Memory Allocations") != std::string::npos);
    EXPECT_TRUE(output.find("KERNEL_ISA") != std::string::npos);
    EXPECT_TRUE(output.find("2048") != std::string::npos);
    EXPECT_TRUE(output.find("100.00%") != std::string::npos);
    EXPECT_TRUE(output.find("System Memory Allocations") == std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportFlagWhenMultipleTypesLoggedThenAllTypesAppearInReport) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto *gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockWddmAllocation allocBuffer(gmmHelper);
    allocBuffer.setAllocationType(AllocationType::buffer);
    allocBuffer.memoryPool = MemoryPool::system64KBPages;
    allocBuffer.setSize(1024u);

    MockWddmAllocation allocLinearStream(gmmHelper);
    allocLinearStream.setAllocationType(AllocationType::linearStream);
    allocLinearStream.memoryPool = MemoryPool::system64KBPages;
    allocLinearStream.setSize(3072u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocBuffer, nullptr);
        logAllocation(fileLogger, &allocLinearStream, nullptr);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Allocation Summary Report") != std::string::npos);
    EXPECT_TRUE(output.find("System Memory Allocations") != std::string::npos);
    EXPECT_TRUE(output.find("BUFFER") != std::string::npos);
    EXPECT_TRUE(output.find("LINEAR_STREAM") != std::string::npos);
    EXPECT_TRUE(output.find("4096") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportFlagNotSetWhenDestructorCalledThenNoSummaryPrinted) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setAllocationType(AllocationType::buffer);
    allocation.memoryPool = MemoryPool::system64KBPages;
    allocation.setSize(1024u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocation, nullptr);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Allocation Summary Report") == std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenAllocationFreedThenPeakMatchesLiveSizeAtPeak) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setAllocationType(AllocationType::buffer);
    allocation.memoryPool = MemoryPool::system64KBPages;
    allocation.setSize(1024u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocation, nullptr);
        logFreeAllocation(fileLogger, &allocation);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Live Memory") != std::string::npos);
    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("BUFFER") != std::string::npos);
    EXPECT_TRUE(output.find("1024") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenMultipleAllocationsFreedThenPeakCapturedAtMaximum) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto *gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockWddmAllocation allocA(gmmHelper);
    allocA.setAllocationType(AllocationType::buffer);
    allocA.memoryPool = MemoryPool::system64KBPages;
    allocA.setSize(1024u);

    MockWddmAllocation allocB(gmmHelper);
    allocB.setAllocationType(AllocationType::linearStream);
    allocB.memoryPool = MemoryPool::system64KBPages;
    allocB.setSize(2048u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocA, nullptr);
        logAllocation(fileLogger, &allocB, nullptr);
        logFreeAllocation(fileLogger, &allocA);
        logFreeAllocation(fileLogger, &allocB);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Live Memory") != std::string::npos);
    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("3072") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenNoAllocationsFreedThenPeakEqualsCumulative) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto *gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockWddmAllocation allocA(gmmHelper);
    allocA.setAllocationType(AllocationType::buffer);
    allocA.memoryPool = MemoryPool::system64KBPages;
    allocA.setSize(1024u);

    MockWddmAllocation allocB(gmmHelper);
    allocB.setAllocationType(AllocationType::linearStream);
    allocB.memoryPool = MemoryPool::system64KBPages;
    allocB.setSize(2048u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocA, nullptr);
        logAllocation(fileLogger, &allocB, nullptr);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Live Memory") != std::string::npos);
    EXPECT_TRUE(output.find("System Memory Allocations") != std::string::npos);
    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("3072") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenLocalMemAllocatedAndFreedThenLocalPeakPrintedWithoutSystemPeak) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setAllocationType(AllocationType::kernelIsa);
    allocation.memoryPool = MemoryPool::localMemory;
    allocation.setSize(2048u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocation, nullptr);
        logFreeAllocation(fileLogger, &allocation);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Live Memory") != std::string::npos);
    EXPECT_TRUE(output.find("Peak Local Memory") != std::string::npos);
    EXPECT_TRUE(output.find("KERNEL_ISA") != std::string::npos);
    EXPECT_TRUE(output.find("2048") != std::string::npos);
    EXPECT_FALSE(output.find("Peak System Memory") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenBothSysAndLocalAllocatedThenBothPeakSectionsPrinted) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto *gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockWddmAllocation allocSys(gmmHelper);
    allocSys.setAllocationType(AllocationType::buffer);
    allocSys.memoryPool = MemoryPool::system64KBPages;
    allocSys.setSize(1024u);

    MockWddmAllocation allocLocal(gmmHelper);
    allocLocal.setAllocationType(AllocationType::kernelIsa);
    allocLocal.memoryPool = MemoryPool::localMemory;
    allocLocal.setSize(2048u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocSys, nullptr);
        logAllocation(fileLogger, &allocLocal, nullptr);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("Peak Local Memory") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenSysPartialFreeAndSmallerReAllocThenPeakPreserved) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto *gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockWddmAllocation allocA(gmmHelper);
    allocA.setAllocationType(AllocationType::buffer);
    allocA.memoryPool = MemoryPool::system64KBPages;
    allocA.setSize(1024u);

    MockWddmAllocation allocB(gmmHelper);
    allocB.setAllocationType(AllocationType::linearStream);
    allocB.memoryPool = MemoryPool::system64KBPages;
    allocB.setSize(1024u);

    MockWddmAllocation allocC(gmmHelper);
    allocC.setAllocationType(AllocationType::buffer);
    allocC.memoryPool = MemoryPool::system64KBPages;
    allocC.setSize(512u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocA, nullptr);
        logAllocation(fileLogger, &allocB, nullptr);
        logFreeAllocation(fileLogger, &allocA);
        logAllocation(fileLogger, &allocC, nullptr);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("2048") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenLocalPartialFreeAndSmallerReAllocThenLocalPeakPreserved) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto *gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    MockWddmAllocation allocA(gmmHelper);
    allocA.setAllocationType(AllocationType::kernelIsa);
    allocA.memoryPool = MemoryPool::localMemory;
    allocA.setSize(1024u);

    MockWddmAllocation allocB(gmmHelper);
    allocB.setAllocationType(AllocationType::privateSurface);
    allocB.memoryPool = MemoryPool::localMemory;
    allocB.setSize(1024u);

    MockWddmAllocation allocC(gmmHelper);
    allocC.setAllocationType(AllocationType::kernelIsa);
    allocC.memoryPool = MemoryPool::localMemory;
    allocC.setSize(512u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocA, nullptr);
        logAllocation(fileLogger, &allocB, nullptr);
        logFreeAllocation(fileLogger, &allocA);
        logAllocation(fileLogger, &allocC, nullptr);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Local Memory") != std::string::npos);
    EXPECT_TRUE(output.find("2048") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenFreeExceedsAllocatedSizeThenNoCrashAndPeakPreserved) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setAllocationType(AllocationType::buffer);
    allocation.memoryPool = MemoryPool::system64KBPages;
    allocation.setSize(100u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocation, nullptr);
        fileLogger.untrackLiveAllocation("BUFFER", 200u, false);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("100") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenFreeCalledForUntrackedTypeThenNoCrashAndUntrackedTypeAbsentFromPeak) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setAllocationType(AllocationType::buffer);
    allocation.memoryPool = MemoryPool::system64KBPages;
    allocation.setSize(1024u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logAllocation(fileLogger, &allocation, nullptr);
        fileLogger.untrackLiveAllocation("LINEAR_STREAM", 512u, false);
        fileLogger.untrackLiveAllocation("KERNEL_ISA", 512u, true);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak System Memory") != std::string::npos);
    EXPECT_TRUE(output.find("BUFFER") != std::string::npos);
    EXPECT_FALSE(output.find("LINEAR_STREAM") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportDisabledWhenLogFreeAllocationCalledThenNoCrash) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    MockWddmAllocation allocation(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper());
    allocation.setAllocationType(AllocationType::buffer);
    allocation.memoryPool = MemoryPool::system64KBPages;
    allocation.setSize(1024u);

    StreamCapture capture;
    capture.captureStdout();
    {
        FullyEnabledFileLogger fileLogger(testFile, flags);
        logFreeAllocation(fileLogger, &allocation);
    }
    std::string output = capture.getCapturedStdout();

    EXPECT_TRUE(output.find("Peak Live Memory") == std::string::npos);
}
