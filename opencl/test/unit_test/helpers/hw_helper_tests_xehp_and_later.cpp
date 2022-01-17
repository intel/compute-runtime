/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/ult_hw_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "engine_node.h"

using HwHelperTestXeHPAndLater = Test<ClDeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, WhenGettingMaxBarriersPerSliceThen32IsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(32u, helper.getMaxBarrierRegisterPerSlice());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenHwHelperWhenGetGpuTimeStampInNSIsCalledThenOnlyLow32BitsFromTimeStampAreUsedAndCorrectValueIsReturned) {

    auto &helper = HwHelper::get(renderCoreFamily);
    auto timeStamp = 0x00ff'ffff'ffff;
    auto frequency = 123456.0;
    auto result = static_cast<uint64_t>((timeStamp & 0xffff'ffff) * frequency);

    EXPECT_EQ(result, helper.getGpuTimeStampInNS(timeStamp, frequency));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, GivenNoCcsNodeThenDefaultEngineTypeIsRcs) {
    hardwareInfo.featureTable.flags.ftrCCSNode = false;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);

    auto expectedEngine = EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, hardwareInfo);

    EXPECT_EQ(expectedEngine, hardwareInfo.capabilityTable.defaultEngineType);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, GiveCcsNodeThenDefaultEngineTypeIsCcs) {
    hardwareInfo.featureTable.flags.ftrCCSNode = true;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_CCS, hardwareInfo.capabilityTable.defaultEngineType);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenXeHPAndLaterPlatformWithLocalMemoryFeatureWhenIsLocalMemoryEnabledIsCalledThenTrueIsReturned) {
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());
    EXPECT_TRUE(helper.isLocalMemoryEnabled(hardwareInfo));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenXeHPAndLaterPlatformWithoutLocalMemoryFeatureWhenIsLocalMemoryEnabledIsCalledThenFalseIsReturned) {
    hardwareInfo.featureTable.flags.ftrLocalMemory = false;

    auto &helper = reinterpret_cast<HwHelperHw<FamilyType> &>(HwHelperHw<FamilyType>::get());
    EXPECT_FALSE(helper.isLocalMemoryEnabled(hardwareInfo));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenXeHPAndLaterPlatformWhenCheckingIfHvAlign4IsRequiredThenReturnFalse) {
    auto &helper = HwHelperHw<FamilyType>::get();
    EXPECT_FALSE(helper.hvAlign4Required());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenXeHPAndLaterPlatformWhenCheckTimestampPacketWriteThenReturnTrue) {
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    EXPECT_TRUE(hwHelper.timestampPacketWriteSupported());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenXeHPAndLaterPlatformWhenCheckAssignEngineRoundRobinSupportedThenReturnTrue) {
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    EXPECT_TRUE(hwHelper.isAssignEngineRoundRobinSupported());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenAllFlagsSetWhenGetGpgpuEnginesThenReturnThreeRcsEnginesFourCcsEnginesAndOneBcsEngine) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = true;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(9u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(9u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[4].first);
    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[5].first); // low priority
    EXPECT_EQ(EngineUsage::LowPriority, engines[5].second);
    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[6].first); // internal
    EXPECT_EQ(EngineUsage::Internal, engines[6].second);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[7].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[8].first);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenBcsDisabledWhenGetGpgpuEnginesThenReturnThreeRcsEnginesFourCcsEngines) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(7u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(7u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[4].first);
    EXPECT_EQ(hwInfo.capabilityTable.defaultEngineType, engines[5].first); // low priority
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[6].first);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenCcsDisabledWhenGetGpgpuEnginesThenReturnRcsAndOneBcsEngine) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(5u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(5u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[4].first);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenCcsDisabledAndNumberOfCcsEnabledWhenGetGpgpuEnginesThenReturnRcsAndOneBcsEngine) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    EXPECT_EQ(5u, device->allEngines.size());
    auto &engines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(5u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[4].first);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenVariousCachesRequestProperMOCSIndexesAreBeingReturned) {
    DebugManagerStateRestore restore;

    auto &helper = HwHelper::get(renderCoreFamily);
    auto gmmHelper = this->pDevice->getRootDeviceEnvironment().getGmmHelper();
    auto expectedMocsForL3off = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    auto expectedMocsForL3on = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
    auto expectedMocsForL3andL1on = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST) >> 1;

    auto mocsIndex = helper.getMocsIndex(*gmmHelper, false, true);
    EXPECT_EQ(expectedMocsForL3off, mocsIndex);

    mocsIndex = helper.getMocsIndex(*gmmHelper, true, false);
    EXPECT_EQ(expectedMocsForL3andL1on, mocsIndex);

    mocsIndex = helper.getMocsIndex(*gmmHelper, true, true);

    EXPECT_EQ(expectedMocsForL3andL1on, mocsIndex);

    DebugManager.flags.ForceL1Caching.set(0u);

    mocsIndex = helper.getMocsIndex(*gmmHelper, true, false);
    EXPECT_EQ(expectedMocsForL3on, mocsIndex);

    mocsIndex = helper.getMocsIndex(*gmmHelper, true, true);
    expectedMocsForL3andL1on = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST) >> 1;

    EXPECT_EQ(expectedMocsForL3andL1on, mocsIndex);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenStoreRegMemCommandWhenAdjustingThenSetRemapEnabled) {
    typename FamilyType::MI_STORE_REGISTER_MEM_CMD storeRegMem = {};

    storeRegMem.setMmioRemapEnable(false);

    GpgpuWalkerHelper<FamilyType>::adjustMiStoreRegMemMode(&storeRegMem);

    EXPECT_TRUE(storeRegMem.getMmioRemapEnable());
}

using PipeControlHelperTestsXeHPAndLater = ::testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, PipeControlHelperTestsXeHPAndLater, WhenAddingPipeControlWAThenCorrectCommandsAreProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    uint8_t buffer[128];
    uint64_t address = 0x1234567887654321;
    HardwareInfo hardwareInfo = *defaultHwInfo;
    bool requiresMemorySynchronization = (MemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(hardwareInfo) > 0) ? true : false;

    for (auto ftrLocalMemory : ::testing::Bool()) {
        LinearStream stream(buffer, 128);
        hardwareInfo.featureTable.flags.ftrLocalMemory = ftrLocalMemory;

        MemorySynchronizationCommands<FamilyType>::addPipeControlWA(stream, address, hardwareInfo);

        if (MemorySynchronizationCommands<FamilyType>::isPipeControlWArequired(hardwareInfo) == false) {
            EXPECT_EQ(0u, stream.getUsed());
            continue;
        }

        GenCmdList cmdList;
        FamilyType::PARSE::parseCommandBuffer(cmdList, stream.getCpuBase(), stream.getUsed());
        EXPECT_EQ(requiresMemorySynchronization ? 2u : 1u, cmdList.size());

        PIPE_CONTROL expectedPipeControl = FamilyType::cmdInitPipeControl;
        expectedPipeControl.setCommandStreamerStallEnable(true);
        UnitTestHelper<FamilyType>::setPipeControlHdcPipelineFlush(expectedPipeControl, true);
        auto it = cmdList.begin();
        auto pPipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        ASSERT_NE(nullptr, pPipeControl);
        EXPECT_TRUE(memcmp(&expectedPipeControl, pPipeControl, sizeof(PIPE_CONTROL)) == 0);

        if (requiresMemorySynchronization) {
            if (UnitTestHelper<FamilyType>::isAdditionalMiSemaphoreWaitRequired(hardwareInfo)) {
                MI_SEMAPHORE_WAIT expectedMiSemaphoreWait;
                EncodeSempahore<FamilyType>::programMiSemaphoreWait(&expectedMiSemaphoreWait, address,
                                                                    EncodeSempahore<FamilyType>::invalidHardwareTag,
                                                                    MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                                    false);
                auto pMiSemaphoreWait = genCmdCast<MI_SEMAPHORE_WAIT *>(*(++it));
                ASSERT_NE(nullptr, pMiSemaphoreWait);
                EXPECT_TRUE(memcmp(&expectedMiSemaphoreWait, pMiSemaphoreWait, sizeof(MI_SEMAPHORE_WAIT)) == 0);
            }
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PipeControlHelperTestsXeHPAndLater, WhenGettingSizeForAdditionalSynchronizationThenCorrectValueIsReturned) {
    HardwareInfo hardwareInfo = *defaultHwInfo;

    EXPECT_EQ(0u, UltMemorySynchronizationCommands<FamilyType>::getSizeForAdditonalSynchronization(hardwareInfo));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PipeControlHelperTestsXeHPAndLater, WhenSettingExtraPipeControlPropertiesThenCorrectValuesAreSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    for (auto ftrLocalMemory : ::testing::Bool()) {
        HardwareInfo hardwareInfo = *defaultHwInfo;
        hardwareInfo.featureTable.flags.ftrLocalMemory = ftrLocalMemory;

        PipeControlArgs args;
        MemorySynchronizationCommands<FamilyType>::setPostSyncExtraProperties(args, hardwareInfo);

        if (ftrLocalMemory) {
            EXPECT_TRUE(args.hdcPipelineFlush);
        } else {
            EXPECT_FALSE(args.hdcPipelineFlush);
        }
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PipeControlHelperTestsXeHPAndLater, whenSettingCacheFlushExtraFieldsThenExpectHdcFlushSet) {
    PipeControlArgs args;

    MemorySynchronizationCommands<FamilyType>::setCacheFlushExtraProperties(args);
    EXPECT_TRUE(args.hdcPipelineFlush);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PipeControlHelperTestsXeHPAndLater, givenRequestedCacheFlushesWhenProgrammingPipeControlThenFlushHdc) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    uint32_t buffer[sizeof(PIPE_CONTROL) * 2] = {};
    LinearStream stream(buffer, sizeof(buffer));

    PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.compressionControlSurfaceCcsFlush = true;
    MemorySynchronizationCommands<FamilyType>::addPipeControl(stream, args);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PipeControlHelperTestsXeHPAndLater, givenDebugVariableSetWhenProgrammingPipeControlThenFlushHdc) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restore;
    DebugManager.flags.FlushAllCaches.set(true);

    uint32_t buffer[sizeof(PIPE_CONTROL) * 2] = {};
    LinearStream stream(buffer, sizeof(buffer));

    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::addPipeControl(stream, args);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_TRUE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PipeControlHelperTestsXeHPAndLater, givenDebugDisableCacheFlushWhenProgrammingPipeControlWithCacheFlushThenExpectDebugOverrideFlushHdc) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restore;
    DebugManager.flags.DoNotFlushCaches.set(true);

    uint32_t buffer[sizeof(PIPE_CONTROL) * 2] = {};
    LinearStream stream(buffer, sizeof(buffer));

    PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.compressionControlSurfaceCcsFlush = true;
    MemorySynchronizationCommands<FamilyType>::addPipeControl(stream, args);

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(buffer);
    EXPECT_FALSE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControl));
    EXPECT_FALSE(pipeControl->getCompressionControlSurfaceCcsFlush());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenHwHelperXeCoreWhenGettingGlobalTimeStampBitsThen32IsReturned) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(helper.getGlobalTimeStampBits(), 32U);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenHwHelperWhenGettingPlanarYuvHeightThenHelperReturnsCorrectValue) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_EQ(helper.getPlanarYuvMaxHeight(), 16128u);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, WhenIsPipeControlWArequiredIsCalledThenCorrectValueIsReturned) {
    auto hwInfo = pDevice->getHardwareInfo();
    for (auto ftrLocalMemory : ::testing::Bool()) {
        hwInfo.featureTable.flags.ftrLocalMemory = ftrLocalMemory;

        EXPECT_EQ(UnitTestHelper<FamilyType>::isPipeControlWArequired(hwInfo),
                  MemorySynchronizationCommands<FamilyType>::isPipeControlWArequired(hwInfo));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, whenGettingPreferenceForSmallKernelsThenCertainThresholdIsTested) {
    DebugManagerStateRestore restorer;
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &helper = HwHelper::get(renderCoreFamily);
    if (HwInfoConfig::get(hwInfo.platform.eProductFamily)->getSteppingFromHwRevId(hwInfo) >= REVISION_B) {
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(512u, hwInfo));
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(10000u, hwInfo));
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(2047u, hwInfo));
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(2048u, hwInfo));
    } else {
        EXPECT_TRUE(helper.preferSmallWorkgroupSizeForKernel(512u, hwInfo));
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(10000u, hwInfo));
        EXPECT_TRUE(helper.preferSmallWorkgroupSizeForKernel(2047u, hwInfo));
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(2048u, hwInfo));

        DebugManager.flags.OverrideKernelSizeLimitForSmallDispatch.set(1u);
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(1u, hwInfo));
        EXPECT_TRUE(helper.preferSmallWorkgroupSizeForKernel(0u, hwInfo));

        DebugManager.flags.OverrideKernelSizeLimitForSmallDispatch.set(0u);
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(1u, hwInfo));
        EXPECT_FALSE(helper.preferSmallWorkgroupSizeForKernel(0u, hwInfo));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwHelperTestXeHPAndLater, givenHwHelperWhenGettingBindlessSurfaceExtendedMessageDescriptorValueThenCorrectValueIsReturned) {
    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    auto value = hwHelper.getBindlessSurfaceExtendedMessageDescriptorValue(0x200);

    typename FamilyType::DataPortBindlessSurfaceExtendedMessageDescriptor messageExtDescriptor = {};
    messageExtDescriptor.setBindlessSurfaceOffset(0x200);

    EXPECT_EQ(messageExtDescriptor.getBindlessSurfaceOffsetToPatch(), value);
    EXPECT_EQ(0x200u, value);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PipeControlHelperTestsXeHPAndLater, givenPostSyncPipeControlWhenSettingWorkloadPartitionFlagThenExpectPipeControlFlagSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    uint8_t buffer[128] = {};
    LinearStream stream(buffer, sizeof(buffer));
    HardwareInfo hardwareInfo = *defaultHwInfo;
    uint64_t gpuAddress = 0xBADA550;
    uint64_t data = 0xABCDEF;

    PipeControlArgs args;
    args.workloadPartitionOffset = true;

    MemorySynchronizationCommands<FamilyType>::addPipeControlAndProgramPostSyncOperation(
        stream,
        POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
        gpuAddress,
        data,
        hardwareInfo,
        args);

    GenCmdList cmdList;
    FamilyType::PARSE::parseCommandBuffer(cmdList, stream.getCpuBase(), stream.getUsed());
    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    bool foundPostSyncPipeControl = false;
    for (size_t i = 0; i < pipeControls.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[i]);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
            EXPECT_EQ(data, pipeControl->getImmediateData());
            EXPECT_TRUE(pipeControl->getWorkloadPartitionIdOffsetEnable());
            foundPostSyncPipeControl = true;
            break;
        }
    }
    EXPECT_TRUE(foundPostSyncPipeControl);
}

using HwInfoConfigTestXeHpAndLater = ::testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, HwInfoConfigTestXeHpAndLater, givenXeHPAndLaterPlatformWhenCheckNewResidencyModelSupportedThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    EXPECT_TRUE(hwInfoConfig->isNewResidencyModelSupported());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwInfoConfigTestXeHpAndLater, whenCapabilityCoherencyFlagSetTrueThenOverrideToFalse) {
    auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    bool coherency = true;
    hwInfoConfig.setCapabilityCoherencyFlag(*defaultHwInfo, coherency);
    EXPECT_FALSE(coherency);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwInfoConfigTestXeHpAndLater, givenXeHPAndLaterPlatformWhenAskedIfTile64With3DSurfaceOnBCSIsSupportedThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    const auto &hwInfo = *defaultHwInfo;

    EXPECT_FALSE(hwInfoConfig.isTile64With3DSurfaceOnBCSSupported(hwInfo));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HwInfoConfigTestXeHpAndLater, givenCLImageFormatsWhenCallingIsFormatRedescribableThenFalseIsReturned) {
    static const cl_image_format redescribeFormats[] = {
        {CL_R, CL_UNSIGNED_INT8},
        {CL_R, CL_UNSIGNED_INT16},
        {CL_R, CL_UNSIGNED_INT32},
        {CL_RG, CL_UNSIGNED_INT32},
        {CL_RGBA, CL_UNSIGNED_INT32},
    };
    MockContext context;
    auto &clHwHelper = ClHwHelper::get(context.getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);

    for (const auto &format : redescribeFormats) {
        EXPECT_EQ(false, clHwHelper.isFormatRedescribable(format));
    }
}
