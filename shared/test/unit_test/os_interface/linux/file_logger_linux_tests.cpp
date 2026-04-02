/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/utilities/logger_neo_only.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/logger_tests.h"

using namespace NEO;

TEST(FileLogger, GivenLogAllocationMemoryPoolFlagThenLogsCorrectInfo) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->initializeMemoryManager();

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    allocation.setGmm(new MockGmm(gmmHelper), 0);

    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(allocation.getDefaultGmm()->resourceParamsData.data());
    gmmResourceParams->Usage = GMM_RESOURCE_USAGE_TYPE_ENUM::GMM_RESOURCE_USAGE_OCL_BUFFER;
    gmmResourceParams->Flags.Info.Cacheable = true;

    auto canonizedGpuAddress = gmmHelper->canonize(0x12345);

    allocation.setCpuPtrAndGpuAddress(&allocation, canonizedGpuAddress);

    MockBufferObject bo(0u, &drm);
    bo.handle.setBoHandle(4);
    bo.setPatIndex(5u);

    allocation.bufferObjects[0] = &bo;

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

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(gpuAddressCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(rootDeviceIndexCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("Type: BUFFER") != std::string::npos);
        EXPECT_TRUE(str.find("Handle: 4") != std::string::npos);
        EXPECT_TRUE(str.find(totalSystemMemoryCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(totalLocalMemoryCheck.str()) != std::string::npos);
    }
}

TEST(FileLogger, givenLogAllocationStdoutWhenLogAllocationThenLogToStdoutInsteadOfFileAndDoNotCreateFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(true);
    flags.LogAllocationStdout.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    allocation.setGmm(new MockGmm(gmmHelper), 0);

    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(allocation.getDefaultGmm()->resourceParamsData.data());
    gmmResourceParams->Usage = GMM_RESOURCE_USAGE_TYPE_ENUM::GMM_RESOURCE_USAGE_OCL_BUFFER;
    gmmResourceParams->Flags.Info.Cacheable = true;

    auto canonizedGpuAddress = gmmHelper->canonize(0x12345);

    allocation.setCpuPtrAndGpuAddress(&allocation, canonizedGpuAddress);

    MockBufferObject bo(0u, &drm);
    bo.handle.setBoHandle(4);
    bo.setPatIndex(5u);

    allocation.bufferObjects[0] = &bo;

    StreamCapture capture;
    capture.captureStdout();
    logAllocation(fileLogger, &allocation, nullptr);
    std::string output = capture.getCapturedStdout();

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " Pool: " << getMemoryPoolString(&allocation);

    std::stringstream gpuAddressCheck;
    gpuAddressCheck << " GPU VA: 0x" << std::hex << allocation.getGpuAddress();

    std::stringstream rootDeviceIndexCheck;
    rootDeviceIndexCheck << " Root index: " << allocation.getRootDeviceIndex();

    EXPECT_TRUE(output.find(threadIDCheck.str()) != std::string::npos);
    EXPECT_TRUE(output.find(memoryPoolCheck.str()) != std::string::npos);
    EXPECT_TRUE(output.find(gpuAddressCheck.str()) != std::string::npos);
    EXPECT_TRUE(output.find(rootDeviceIndexCheck.str()) != std::string::npos);
    EXPECT_TRUE(output.find("Type: BUFFER") != std::string::npos);
    EXPECT_TRUE(output.find("Handle: 4") != std::string::npos);
    EXPECT_TRUE(output.find("\n") != std::string::npos);

    logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
}

TEST(FileLogger, GivenDrmAllocationWithoutBOThenNoHandleLogged) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    allocation.setGmm(new MockGmm(gmmHelper), 0);

    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(allocation.getDefaultGmm()->resourceParamsData.data());
    gmmResourceParams->Usage = GMM_RESOURCE_USAGE_TYPE_ENUM::GMM_RESOURCE_USAGE_OCL_BUFFER;
    gmmResourceParams->Flags.Info.Cacheable = true;
    logAllocation(fileLogger, &allocation, nullptr);
    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " Pool: " << getMemoryPoolString(&allocation);

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("Type: BUFFER") != std::string::npos);
        EXPECT_FALSE(str.find("Handle: 4") != std::string::npos);
    }
}

TEST(FileLogger, GivenLogAllocationMemoryPoolFlagSetFalseThenAllocationIsNotLogged) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(false);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    logAllocation(fileLogger, &allocation, nullptr);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " Pool: " << getMemoryPoolString(&allocation);

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_FALSE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("Type: BUFFER") != std::string::npos);
    }
}

TEST(FileLogger, givenLogAllocationSummaryReportFlagWhenSysMemAllocationLoggedThenSummaryPrintedToStdoutOnDestruction) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
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

    MockDrmAllocation allocation(0u, AllocationType::kernelIsa, MemoryPool::localMemory);
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

    MockDrmAllocation allocBuffer(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    allocBuffer.setSize(1024u);
    MockDrmAllocation allocLinearStream(0u, AllocationType::linearStream, MemoryPool::system64KBPages);
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

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
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

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
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

    MockDrmAllocation allocA(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    allocA.setSize(1024u);
    MockDrmAllocation allocB(0u, AllocationType::linearStream, MemoryPool::system64KBPages);
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

    MockDrmAllocation allocA(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    allocA.setSize(1024u);
    MockDrmAllocation allocB(0u, AllocationType::linearStream, MemoryPool::system64KBPages);
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
    size_t cumulativePos = output.find("System Memory Allocations");
    size_t peakPos = output.find("Peak System Memory");
    EXPECT_TRUE(cumulativePos != std::string::npos);
    EXPECT_TRUE(peakPos != std::string::npos);
    EXPECT_TRUE(output.find("3072") != std::string::npos);
}

TEST(FileLogger, givenLogAllocationSummaryReportWhenLocalMemAllocatedAndFreedThenLocalPeakPrintedWithoutSystemPeak) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationSummaryReport.set(true);

    MockDrmAllocation allocation(0u, AllocationType::kernelIsa, MemoryPool::localMemory);
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

    MockDrmAllocation allocSys(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    allocSys.setSize(1024u);
    MockDrmAllocation allocLocal(0u, AllocationType::kernelIsa, MemoryPool::localMemory);
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

    MockDrmAllocation allocA(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    allocA.setSize(1024u);
    MockDrmAllocation allocB(0u, AllocationType::linearStream, MemoryPool::system64KBPages);
    allocB.setSize(1024u);
    MockDrmAllocation allocC(0u, AllocationType::buffer, MemoryPool::system64KBPages);
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

    MockDrmAllocation allocA(0u, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocA.setSize(1024u);
    MockDrmAllocation allocB(0u, AllocationType::privateSurface, MemoryPool::localMemory);
    allocB.setSize(1024u);
    MockDrmAllocation allocC(0u, AllocationType::kernelIsa, MemoryPool::localMemory);
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

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
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

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
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

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
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
