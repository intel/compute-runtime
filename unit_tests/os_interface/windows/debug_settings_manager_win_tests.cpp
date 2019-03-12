/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "test.h"
#include "unit_tests/os_interface/debug_settings_manager_fixture.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"

TEST(DebugSettingsManager, GivenDebugSettingsManagerWithLogAllocationsThenLogsCorrectInfo) {
    FullyEnabledTestDebugManager debugManager;

    // Log file not created
    bool logFileCreated = fileExists(debugManager.getLogFileName());
    EXPECT_FALSE(logFileCreated);

    debugManager.flags.LogAllocationMemoryPool.set(true);

    MockWddmAllocation allocation;
    allocation.handle = 4;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    allocation.memoryPool = MemoryPool::System64KBPages;
    auto gmm = std::make_unique<Gmm>(nullptr, 0, false);
    allocation.setDefaultGmm(gmm.get());
    allocation.getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly = 0;

    debugManager.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("Handle: 4") != std::string::npos);
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

    MockWddmAllocation allocation;
    allocation.handle = 4;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    allocation.memoryPool = MemoryPool::System64KBPages;
    auto gmm = std::make_unique<Gmm>(nullptr, 0, false);
    allocation.setDefaultGmm(gmm.get());
    allocation.getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly = 0;

    debugManager.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

    if (debugManager.wasFileCreated(debugManager.getLogFileName())) {
        auto str = debugManager.getFileString(debugManager.getLogFileName());
        EXPECT_FALSE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("Handle: 4") != std::string::npos);
        EXPECT_FALSE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("AllocationType: BUFFER") != std::string::npos);
        EXPECT_FALSE(str.find("NonLocalOnly: 0") != std::string::npos);
    }
}
