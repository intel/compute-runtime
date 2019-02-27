/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/debug_settings_manager.h"
#include "test.h"
#include "unit_tests/mocks/linux/mock_drm_allocation.h"
#include "unit_tests/os_interface/debug_settings_manager_fixture.h"

TEST(DebugSettingsManager, GivenDebugSettingsManagerWithLogAllocationsThenLogsCorrectInfo) {
    FullyEnabledTestDebugManager debugManager;

    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    debugManager.flags.LogAllocationMemoryPool.set(true);

    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::System64KBPages);

    MockBufferObject bo;
    bo.handle = 4;

    allocation.bo = &bo;

    debugManager.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("AllocationType: BUFFER") != std::string::npos);
        EXPECT_TRUE(str.find("Handle: 4") != std::string::npos);
    }
}

TEST(DebugSettingsManager, GivenDebugSettingsManagerWithDrmAllocationWithoutBOThenNoHandleLogged) {
    FullyEnabledTestDebugManager debugManager;

    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    debugManager.flags.LogAllocationMemoryPool.set(true);

    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::System64KBPages);

    debugManager.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("AllocationType: BUFFER") != std::string::npos);
        EXPECT_FALSE(str.find("Handle: 4") != std::string::npos);
    }
}

TEST(DebugSettingsManager, GivenDebugSettingsManagerWithoutLogAllocationsThenAllocationIsNotLogged) {
    FullyEnabledTestDebugManager debugManager;

    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    debugManager.flags.LogAllocationMemoryPool.set(false);

    MockDrmAllocation allocation(GraphicsAllocation::AllocationType::BUFFER, MemoryPool::System64KBPages);

    debugManager.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_FALSE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("AllocationType: BUFFER") != std::string::npos);
    }
}
