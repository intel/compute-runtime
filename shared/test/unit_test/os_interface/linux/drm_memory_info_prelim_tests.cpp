/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

struct DrmVmTestFixture {
    void setUp() {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        testHwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
        testHwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = true;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile0 =
            testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile1 =
                testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile2 =
                    testHwInfo->gtSystemInfo.MultiTileArchInfo.Tile3 = 1;
        testHwInfo->gtSystemInfo.MultiTileArchInfo.TileMask = 0b1111;
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        drm = std::make_unique<DrmQueryMock>(*executionEnvironment->rootDeviceEnvironments[0]);
        ASSERT_NE(nullptr, drm);
    }

    void tearDown() {}

    DebugManagerStateRestore restorer;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmQueryMock> drm;

    NEO::HardwareInfo *testHwInfo = nullptr;

    static constexpr uint8_t tileCount = 4u;
};

using DrmVmTestTest = Test<DrmVmTestFixture>;

HWTEST2_F(DrmVmTestTest, givenNewMemoryInfoQuerySupportedWhenCreatingVirtualMemoryThenVmCreatedUsingNewRegion, IsAtMostXeCore) {
    debugManager.flags.EnableLocalMemory.set(1);
    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    drm->queryEngineInfo();
    EXPECT_EQ(5u, drm->ioctlCallsCount);
    drm->ioctlCallsCount = 0u;

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(1u + tileCount, memoryInfo->getDrmRegionInfos().size());

    drm->ioctlCount.reset();
    bool ret = drm->createVirtualMemoryAddressSpace(tileCount);
    EXPECT_TRUE(ret);
    EXPECT_EQ(tileCount, drm->ioctlCount.gemVmCreate.load());
    EXPECT_NE(0ull, drm->receivedGemVmControl.extensions);
}

HWTEST2_F(DrmVmTestTest, givenNewMemoryInfoQuerySupportedAndDebugKeyDisabledWhenCreatingVirtualMemoryThenVmCreatedNotUsingRegion, IsAtMostXeCore) {
    debugManager.flags.UseTileMemoryBankInVirtualMemoryCreation.set(0);

    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);
    drm->ioctlCallsCount = 0u;
    drm->queryEngineInfo();

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(1u + tileCount, memoryInfo->getDrmRegionInfos().size());

    drm->ioctlCount.reset();
    bool ret = drm->createVirtualMemoryAddressSpace(tileCount);
    EXPECT_TRUE(ret);
    EXPECT_EQ(tileCount, drm->ioctlCount.gemVmCreate.load());
    EXPECT_EQ(0ull, drm->receivedGemVmControl.extensions);
}

HWTEST2_F(DrmVmTestTest, givenNewMemoryInfoQuerySupportedWhenCreatingVirtualMemoryFailsThenExpectDebugInformation, IsAtMostXeCore) {
    NEO::debugManager.flags.PrintDebugMessages.set(1);
    NEO::debugManager.flags.EnableLocalMemory.set(1);
    drm->storedRetValForVmCreate = 1;

    drm->memoryInfoQueried = false;
    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);
    drm->ioctlCallsCount = 0u;
    drm->queryEngineInfo();

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(1u + tileCount, memoryInfo->getDrmRegionInfos().size());

    drm->ioctlCount.reset();
    StreamCapture capture;
    capture.captureStderr();
    bool ret = drm->createVirtualMemoryAddressSpace(tileCount);
    EXPECT_FALSE(ret);
    EXPECT_EQ(1, drm->ioctlCount.gemVmCreate.load());
    EXPECT_NE(0ull, drm->receivedGemVmControl.extensions);

    std::string output = capture.getCapturedStderr();
    auto pos = output.find("INFO: Cannot create Virtual Memory at memory bank");
    EXPECT_NE(std::string::npos, pos);
}
