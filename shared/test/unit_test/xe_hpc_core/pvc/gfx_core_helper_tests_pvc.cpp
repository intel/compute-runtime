/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {

using GfxCoreHelperTestsPvc = GfxCoreHelperTest;

PVCTEST_F(GfxCoreHelperTestsPvc, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_B,
        REVISION_C,
        REVISION_D,
        CommonConstants::invalidStepping,
    };

    const auto &productHelper = getHelper<ProductHelper>();

    for (auto stepping : steppings) {
        hardwareInfo.platform.usDeviceID = pvcXlDeviceIds[0];
        hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (stepping == REVISION_A0) {
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, productHelper));
        } else if (stepping == REVISION_B) {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
            EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, productHelper));
        } else {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_C, hardwareInfo, productHelper));
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_C, hardwareInfo, productHelper));
        }

        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_C, REVISION_A0, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_C, REVISION_B, hardwareInfo, productHelper));

        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_D, REVISION_A0, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, productHelper));
        EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A1, REVISION_A0, hardwareInfo, productHelper));
    }
}

PVCTEST_F(GfxCoreHelperTestsPvc, givenDefaultMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    EXPECT_EQ(sizeof(typename FamilyType::MI_MEM_FENCE), MemorySynchronizationCommands<XeHpcCoreFamily>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

PVCTEST_F(GfxCoreHelperTestsPvc, givenDebugMemorySynchronizationCommandsWhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);

    EXPECT_EQ(2 * sizeof(typename FamilyType::MI_MEM_FENCE), MemorySynchronizationCommands<XeHpcCoreFamily>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pDevice->getRootDeviceEnvironment()));
}

PVCTEST_F(GfxCoreHelperTestsPvc, givenRevisionIdWhenGetComputeUnitsUsedForScratchThenReturnValidValue) {
    auto &gfxCoreHelper = pDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
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
        EXPECT_EQ(expectedValue * testInput.expectedRatio, gfxCoreHelper.getComputeUnitsUsedForScratch(pDevice->getRootDeviceEnvironment()));
    }
}

PVCTEST_F(GfxCoreHelperTestsPvc, givenMemorySynchronizationCommandsWhenAddingSynchronizationThenCorrectMethodIsUsed) {
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

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    auto &hardwareInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    uint8_t buffer[128] = {};
    uint64_t gpuAddress = 0x12345678;

    for (auto &testInput : testInputs) {
        hardwareInfo.platform.usRevId = testInput.revisionId;
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(
            testInput.programGlobalFenceAsMiMemFenceCommandInCommandStream);

        LinearStream commandStream(buffer, 128);
        auto synchronizationSize = MemorySynchronizationCommands<FamilyType>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, rootDeviceEnvironment);

        MemorySynchronizationCommands<FamilyType>::addAdditionalSynchronization(commandStream, gpuAddress, NEO::FenceType::release, rootDeviceEnvironment);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStream);
        EXPECT_EQ(1u, hwParser.cmdList.size());

        if (testInput.expectMiSemaphoreWait) {
            EXPECT_EQ(NEO::EncodeSemaphore<FamilyType>::getSizeMiSemaphoreWait(), synchronizationSize);

            auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*hwParser.cmdList.begin());
            ASSERT_NE(nullptr, semaphoreCmd);
            EXPECT_EQ(static_cast<uint32_t>(-2), semaphoreCmd->getSemaphoreDataDword());
            EXPECT_EQ(gpuAddress, semaphoreCmd->getSemaphoreGraphicsAddress());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, semaphoreCmd->getCompareOperation());
        } else {
            EXPECT_EQ(sizeof(MI_MEM_FENCE), synchronizationSize);

            auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*hwParser.cmdList.begin());
            ASSERT_NE(nullptr, fenceCmd);
            EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
        }
    }
}

PVCTEST_F(GfxCoreHelperTestsPvc, GivenCooperativeEngineSupportedAndNotUsedWhenAdjustMaxWorkGroupCountIsCalledThenSmallerValueIsReturned) {

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    hwInfo.capabilityTable.defaultEngineType = aub_stream::EngineType::ENGINE_CCS;
    hwInfo.featureTable.flags.ftrRcsNode = false;

    auto tilePartsForConcurrentKernels = PVC::numberOfpartsInTileForConcurrentKernels;
    auto passedMaxWorkGroupCount = 1024;

    uint32_t revisions[] = {REVISION_A0, REVISION_B, REVISION_C};
    for (auto &revision : revisions) {
        auto hwRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        hwInfo.platform.usRevId = hwRevId;
        for (auto engineGroupType : {EngineGroupType::renderCompute, EngineGroupType::compute, EngineGroupType::cooperativeCompute}) {
            for (auto isRcsEnabled : ::testing::Bool()) {
                hwInfo.featureTable.flags.ftrRcsNode = isRcsEnabled;
                bool disallowDispatch = (engineGroupType == EngineGroupType::renderCompute ||
                                         (engineGroupType == EngineGroupType::compute && isRcsEnabled)) &&
                                        productHelper.isCooperativeEngineSupported(hwInfo);
                if (disallowDispatch) {
                    EXPECT_EQ(1u, gfxCoreHelper.adjustMaxWorkGroupCount(passedMaxWorkGroupCount, engineGroupType, rootDeviceEnvironment));
                } else {
                    for (uint32_t ccsCount : {1, 2, 4}) {
                        hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = ccsCount;
                        tilePartsForConcurrentKernels = ccsCount == 1   ? 1
                                                        : ccsCount == 2 ? 4
                                                                        : 8;
                        EXPECT_EQ(passedMaxWorkGroupCount / tilePartsForConcurrentKernels, gfxCoreHelper.adjustMaxWorkGroupCount(passedMaxWorkGroupCount, engineGroupType, rootDeviceEnvironment));
                    }
                }
            }
        }
    }
}
} // namespace NEO
