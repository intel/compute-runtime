/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/utilities/logger_tests.h"

using namespace NEO;

TEST(FileLogger, GivenLogAllocationMemoryPoolFlagThenLogsCorrectInfo) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::System64KBPages);
    auto gmmHelper = std::make_unique<GmmHelper>(nullptr, defaultHwInfo.get());
    auto canonizedGpuAddress = gmmHelper->canonize(0x12345);

    allocation.setCpuPtrAndGpuAddress(&allocation, canonizedGpuAddress);

    MockBufferObject bo(&drm);
    bo.handle = 4;

    allocation.bufferObjects[0] = &bo;

    fileLogger.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << getMemoryPoolString(&allocation);

    std::stringstream gpuAddressCheck;
    gpuAddressCheck << " GPU address: 0x" << std::hex << allocation.getGpuAddress();

    std::stringstream rootDeviceIndexCheck;
    rootDeviceIndexCheck << " Root device index: " << allocation.getRootDeviceIndex();

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(gpuAddressCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(rootDeviceIndexCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("AllocationType: BUFFER") != std::string::npos);
        EXPECT_TRUE(str.find("Handle: 4") != std::string::npos);
    }
}

TEST(FileLogger, givenLogAllocationStdoutWhenLogAllocationThenLogToStdoutInsteadOfFileAndDoNotCreateFile) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(true);
    flags.LogAllocationStdout.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::System64KBPages);
    auto gmmHelper = std::make_unique<GmmHelper>(nullptr, defaultHwInfo.get());
    auto canonizedGpuAddress = gmmHelper->canonize(0x12345);

    allocation.setCpuPtrAndGpuAddress(&allocation, canonizedGpuAddress);

    MockBufferObject bo(&drm);
    bo.handle = 4;

    allocation.bufferObjects[0] = &bo;

    testing::internal::CaptureStdout();
    fileLogger.logAllocation(&allocation);
    std::string output = testing::internal::GetCapturedStdout();

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << getMemoryPoolString(&allocation);

    std::stringstream gpuAddressCheck;
    gpuAddressCheck << " GPU address: 0x" << std::hex << allocation.getGpuAddress();

    std::stringstream rootDeviceIndexCheck;
    rootDeviceIndexCheck << " Root device index: " << allocation.getRootDeviceIndex();

    EXPECT_TRUE(output.find(threadIDCheck.str()) != std::string::npos);
    EXPECT_TRUE(output.find(memoryPoolCheck.str()) != std::string::npos);
    EXPECT_TRUE(output.find(gpuAddressCheck.str()) != std::string::npos);
    EXPECT_TRUE(output.find(rootDeviceIndexCheck.str()) != std::string::npos);
    EXPECT_TRUE(output.find("AllocationType: BUFFER") != std::string::npos);
    EXPECT_TRUE(output.find("Handle: 4") != std::string::npos);
    EXPECT_TRUE(output.find("\n") != std::string::npos);

    logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
}

TEST(FileLogger, GivenDrmAllocationWithoutBOThenNoHandleLogged) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::System64KBPages);

    fileLogger.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << getMemoryPoolString(&allocation);

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("AllocationType: BUFFER") != std::string::npos);
        EXPECT_FALSE(str.find("Handle: 4") != std::string::npos);
    }
}

TEST(FileLogger, GivenLogAllocationMemoryPoolFlagSetFalseThenAllocationIsNotLogged) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(false);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::System64KBPages);

    fileLogger.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << getMemoryPoolString(&allocation);

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_FALSE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("AllocationType: BUFFER") != std::string::npos);
    }
}
