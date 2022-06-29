/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/xe_hpc_core/xe_hpc_core_test_ocl_fixtures.h"
namespace NEO {

using HwHelperTestsPvcXt = Test<HwHelperTestsXeHpcCore>;
PVCTEST_F(HwHelperTestsPvcXt, givenSingleTileCsrOnPvcXtWhenAllocatingCsrSpecificAllocationsAndIsNotBaseDieA0ThenStoredInProperMemoryPool) {
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usDeviceID = PVC_XT_IDS.front();
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo); // not BD A0
    checkIfSingleTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInProperMemoryPool(&hwInfo);
}

PVCTEST_F(HwHelperTestsPvcXt, givenMultiTileCsrOnPvcWhenAllocatingCsrSpecificAllocationsAndIsNotBaseDieA0ThenStoredInLocalMemoryPool) {
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    hwInfo.platform.usDeviceID = PVC_XT_IDS.front();
    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo); // not BD A0
    checkIfMultiTileCsrWhenAllocatingCsrSpecificAllocationsThenStoredInLocalMemoryPool(&hwInfo);
}

using HwHelperTestsPvc = Test<HwHelperTestsXeHpcCore>;
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
    using MI_SEMAPHORE_WAIT = typename XE_HPC_COREFamily::MI_SEMAPHORE_WAIT;

    EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), MemorySynchronizationCommands<XE_HPC_COREFamily>::getSizeForAdditonalSynchronization(*defaultHwInfo));
}

PVCTEST_F(HwHelperTestsPvc, givenDebugMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);
    using MI_SEMAPHORE_WAIT = typename XE_HPC_COREFamily::MI_SEMAPHORE_WAIT;

    EXPECT_EQ(2 * sizeof(MI_SEMAPHORE_WAIT), MemorySynchronizationCommands<XE_HPC_COREFamily>::getSizeForAdditonalSynchronization(*defaultHwInfo));
}

PVCTEST_F(HwHelperTestsPvc, givenRevisionIdWhenGetComputeUnitsUsedForScratchThenReturnValidValue) {
    auto &helper = HwHelper::get(IGFX_XE_HPC_CORE);
    auto hwInfo = *defaultHwInfo;
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
        EXPECT_EQ(expectedValue * testInput.expectedRatio, helper.getComputeUnitsUsedForScratch(&hwInfo));
    }
}
} // namespace NEO