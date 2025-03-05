/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/utilities/logger_neo_only.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/windows/mock_wddm_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/logger_tests.h"

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
    allocation.getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly = 0;
    allocation.getDefaultGmm()->resourceParams.Usage = GMM_RESOURCE_USAGE_HEAP_STATELESS_DATA_PORT_L1_CACHED;
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

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("Handle: 4") != std::string::npos);
        EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(gpuAddressCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(rootDeviceIndexCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("UNKNOWN GMM USAGE TYPE 40") != std::string::npos);
        EXPECT_TRUE(str.find("Type: BUFFER") != std::string::npos);
        EXPECT_TRUE(str.find("Size: 777") != std::string::npos);
        EXPECT_TRUE(str.find(totalSystemMemoryCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(totalLocalMemoryCheck.str()) != std::string::npos);
    }
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
    allocation.getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly = 0;

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
