/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/debug_settings_manager.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/os_interface/debug_settings_manager_fixture.h"
#include "test.h"

TEST(DebugSettingsManager, GivenDebugSettingsManagerWithLogAllocationsThenLogsCorrectInfo) {
    FullyEnabledTestDebugManager debugManager;

    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    debugManager.flags.LogAllocationMemoryPool.set(true);

    MockGraphicsAllocation allocation;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    allocation.overrideMemoryPool(MemoryPool::System64KBPages);

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
    }
}

TEST(DebugSettingsManager, GivenDebugSettingsManagerWithoutLogAllocationsThenAllocationIsNotLogged) {
    FullyEnabledTestDebugManager debugManager;

    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    debugManager.flags.LogAllocationMemoryPool.set(false);

    MockGraphicsAllocation allocation;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    allocation.overrideMemoryPool(MemoryPool::System64KBPages);

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
