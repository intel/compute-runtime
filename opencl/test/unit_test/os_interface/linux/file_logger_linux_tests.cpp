/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/test/unit_test/mocks/linux/mock_drm_allocation.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock.h"
#include "opencl/test/unit_test/utilities/file_logger_tests.h"
#include "test.h"

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

    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::System64KBPages);

    allocation.setCpuPtrAndGpuAddress(&allocation, 0x12345);

    MockBufferObject bo(&drm);
    bo.handle = 4;

    allocation.bufferObjects[0] = &bo;

    fileLogger.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

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

TEST(FileLogger, GivenDrmAllocationWithoutBOThenNoHandleLogged) {
    std::string testFile = "testfile";
    DebugVariables flags;
    flags.LogAllocationMemoryPool.set(true);
    FullyEnabledFileLogger fileLogger(testFile, flags);

    // Log file not created
    bool logFileCreated = fileExists(fileLogger.getLogFileName());
    EXPECT_FALSE(logFileCreated);
    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::System64KBPages);

    fileLogger.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

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

    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::System64KBPages);

    fileLogger.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_FALSE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("AllocationType: BUFFER") != std::string::npos);
    }
}
