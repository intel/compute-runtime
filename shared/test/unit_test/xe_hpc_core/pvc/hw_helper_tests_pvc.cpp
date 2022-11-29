/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

using HwHelperTestsPvc = HwHelperTest;

PVCTEST_F(HwHelperTestsPvc, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        REVISION_C,
        REVISION_D,
        CommonConstants::invalidStepping,
    };

    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (stepping == REVISION_A0) {
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
        } else if (stepping == REVISION_B) {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
        } else {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo));
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo));
        }

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_C, REVISION_B, hardwareInfo));

        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_D, REVISION_A0, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
        EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A1, REVISION_A0, hardwareInfo));
    }
}

PVCTEST_F(HwHelperTestsPvc, givenDefaultMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    using MI_SEMAPHORE_WAIT = typename XeHpcCoreFamily::MI_SEMAPHORE_WAIT;

    EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), MemorySynchronizationCommands<XeHpcCoreFamily>::getSizeForAdditonalSynchronization(*defaultHwInfo));
}

PVCTEST_F(HwHelperTestsPvc, givenDebugMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    using MI_SEMAPHORE_WAIT = typename XeHpcCoreFamily::MI_SEMAPHORE_WAIT;

    EXPECT_EQ(2 * sizeof(MI_SEMAPHORE_WAIT), MemorySynchronizationCommands<XeHpcCoreFamily>::getSizeForAdditonalSynchronization(*defaultHwInfo));
}

PVCTEST_F(HwHelperTestsPvc, givenRevisionIdWhenGetComputeUnitsUsedForScratchThenReturnValidValue) {
    auto &coreHelper = pDevice->getRootDeviceEnvironment().getHelper<CoreHelper>();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo.gtSystemInfo.EUCount *= 2;

    uint32_t expectedValue = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice;

    struct {
        unsigned short revId;
        uint32_t expectedRatio;
    } testInputs[] = {
        {0x0, 8},
        {0x1, 8},
        {0x3, 16},
        {0x5, 16},
        {0x6, 16},
        {0x7, 16},
    };

    for (auto &testInput : testInputs) {
        hwInfo.platform.usRevId = testInput.revId;
        EXPECT_EQ(expectedValue * testInput.expectedRatio, coreHelper.getComputeUnitsUsedForScratch(pDevice->getRootDeviceEnvironment()));
    }
}

PVCTEST_F(HwHelperTestsPvc, givenMemorySynchronizationCommandsWhenAddingSynchronizationThenCorrectMethodIsUsed) {
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    struct {
        unsigned short revisionId;
        int32_t programGlobalFenceAsMiMemFenceCommandInCommandStream;
        bool expectMiSemaphoreWait;
    } testInputs[] = {
        {0x0, -1, true},
        {0x3, -1, false},
        {0x0, 0, true},
        {0x3, 0, true},
        {0x0, 1, false},
        {0x3, 1, false},
    };

    DebugManagerStateRestore debugRestorer;
    auto hardwareInfo = *defaultHwInfo;

    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    uint8_t buffer[128] = {};
    uint64_t gpuAddress = 0x12345678;

    for (auto &testInput : testInputs) {
        hardwareInfo.platform.usRevId = testInput.revisionId;
        DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(
            testInput.programGlobalFenceAsMiMemFenceCommandInCommandStream);

        LinearStream commandStream(buffer, 128);
        auto synchronizationSize = MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(hardwareInfo);

        MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, gpuAddress, false, hardwareInfo);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStream);
        EXPECT_EQ(1u, hwParser.cmdList.size());

        if (testInput.expectMiSemaphoreWait) {
            EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), synchronizationSize);

            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*hwParser.cmdList.begin());
            ASSERT_NE(nullptr, semaphoreCmd);
            EXPECT_EQ(static_cast<uint32_t>(-2), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());
        } else {
            EXPECT_EQ(sizeof(MI_MEM_FENCE), synchronizationSize);

            auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*hwParser.cmdList.begin());
            ASSERT_NE(nullptr, fenceCmd);
            EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE, fenceCmd->getFenceType());
        }
    }
}
} // namespace NEO
