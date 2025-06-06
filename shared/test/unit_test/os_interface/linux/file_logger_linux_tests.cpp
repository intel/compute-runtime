/*
 * Copyright (C) 2019-2025 Intel Corporation
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
    fileLogger.useRealFiles(false);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->initializeMemoryManager();

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    allocation.setGmm(new MockGmm(gmmHelper), 0);

    allocation.getDefaultGmm()->resourceParams.Usage = GMM_RESOURCE_USAGE_TYPE_ENUM::GMM_RESOURCE_USAGE_OCL_BUFFER;
    allocation.getDefaultGmm()->resourceParams.Flags.Info.Cacheable = true;

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
    fileLogger.useRealFiles(false);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    allocation.setGmm(new MockGmm(gmmHelper), 0);

    allocation.getDefaultGmm()->resourceParams.Usage = GMM_RESOURCE_USAGE_TYPE_ENUM::GMM_RESOURCE_USAGE_OCL_BUFFER;
    allocation.getDefaultGmm()->resourceParams.Flags.Info.Cacheable = true;

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
    fileLogger.useRealFiles(false);

    // Log file not created
    bool logFileCreated = virtualFileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::system64KBPages);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper();

    allocation.setGmm(new MockGmm(gmmHelper), 0);

    allocation.getDefaultGmm()->resourceParams.Usage = GMM_RESOURCE_USAGE_TYPE_ENUM::GMM_RESOURCE_USAGE_OCL_BUFFER;
    allocation.getDefaultGmm()->resourceParams.Flags.Info.Cacheable = true;
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
    fileLogger.useRealFiles(false);

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
