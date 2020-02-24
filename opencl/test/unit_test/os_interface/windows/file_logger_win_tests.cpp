/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/os_interface/windows/mock_wddm_allocation.h"
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

    MockWddmAllocation allocation;
    allocation.handle = 4;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    allocation.memoryPool = MemoryPool::System64KBPages;
    auto gmm = std::make_unique<Gmm>(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), nullptr, 0, false);
    allocation.setDefaultGmm(gmm.get());
    allocation.getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly = 0;

    fileLogger.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_TRUE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("Handle: 4") != std::string::npos);
        EXPECT_TRUE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_TRUE(str.find("AllocationType: BUFFER") != std::string::npos);
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

    MockWddmAllocation allocation;
    allocation.handle = 4;
    allocation.setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
    allocation.memoryPool = MemoryPool::System64KBPages;
    auto gmm = std::make_unique<Gmm>(platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->getGmmClientContext(), nullptr, 0, false);
    allocation.setDefaultGmm(gmm.get());
    allocation.getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly = 0;

    fileLogger.logAllocation(&allocation);

    std::thread::id thisThread = std::this_thread::get_id();

    std::stringstream threadIDCheck;
    threadIDCheck << " ThreadID: " << thisThread;

    std::stringstream memoryPoolCheck;
    memoryPoolCheck << " MemoryPool: " << allocation.getMemoryPool();

    if (fileLogger.wasFileCreated(fileLogger.getLogFileName())) {
        auto str = fileLogger.getFileString(fileLogger.getLogFileName());
        EXPECT_FALSE(str.find(threadIDCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("Handle: 4") != std::string::npos);
        EXPECT_FALSE(str.find(memoryPoolCheck.str()) != std::string::npos);
        EXPECT_FALSE(str.find("AllocationType: BUFFER") != std::string::npos);
        EXPECT_FALSE(str.find("NonLocalOnly: 0") != std::string::npos);
    }
}
