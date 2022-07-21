/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "hw_cmds_xe_hpg_core_base.h"

using HwHelperTestXeHpgCore = HwHelperTest;

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenDifferentBufferSizesWhenEnableStatelessCompressionThenEveryBufferSizeIsSuitableForCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &helper = HwHelper::get(renderCoreFamily);

    const size_t sizesToCheck[] = {1, 128, 256, 1024, 2048};
    for (size_t size : sizesToCheck) {
        EXPECT_TRUE(helper.isBufferSizeSuitableForCompression(size, *defaultHwInfo));
    }
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenDebugFlagWhenCheckingIfBufferIsSuitableThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &helper = HwHelper::get(renderCoreFamily);

    const size_t sizesToCheck[] = {1, 128, 256, 1024, 2048};

    for (int32_t debugFlag : {-1, 0, 1}) {
        DebugManager.flags.OverrideBufferSuitableForRenderCompression.set(debugFlag);

        for (size_t size : sizesToCheck) {
            if (debugFlag == 1) {
                EXPECT_TRUE(helper.isBufferSizeSuitableForCompression(size, *defaultHwInfo));
            } else {
                EXPECT_FALSE(helper.isBufferSizeSuitableForCompression(size, *defaultHwInfo));
            }
        }
    }
}

using HwInfoConfigTestXeHpgCore = ::testing::Test;

XE_HPG_CORETEST_F(HwInfoConfigTestXeHpgCore, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
    DebugManagerStateRestore restore;
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.capabilityTable.blitterOperationsSupported);

    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_TRUE(hwInfo.capabilityTable.blitterOperationsSupported);
}

using LriHelperTestsXeHpgCore = ::testing::Test;

XE_HPG_CORETEST_F(LriHelperTestsXeHpgCore, whenProgrammingLriCommandThenExpectMmioRemapEnableCorrectlySet) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    std::unique_ptr<uint8_t> buffer(new uint8_t[128]);

    LinearStream stream(buffer.get(), 128);
    uint32_t address = 0x8888;
    uint32_t data = 0x1234;

    auto expectedLri = FamilyType::cmdInitLoadRegisterImm;
    EXPECT_FALSE(expectedLri.getMmioRemapEnable());
    expectedLri.setRegisterOffset(address);
    expectedLri.setDataDword(data);
    expectedLri.setMmioRemapEnable(true);

    LriHelper<FamilyType>::program(&stream, address, data, true);
    MI_LOAD_REGISTER_IMM *lri = genCmdCast<MI_LOAD_REGISTER_IMM *>(buffer.get());
    ASSERT_NE(nullptr, lri);

    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), stream.getUsed());
    EXPECT_EQ(lri, stream.getCpuBase());
    EXPECT_TRUE(memcmp(lri, &expectedLri, sizeof(MI_LOAD_REGISTER_IMM)) == 0);
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenAllocDataWhenSetExtraAllocationDataThenSetLocalMemForProperTypes) {
    auto &hwHelper = HwHelper::get(renderCoreFamily);

    for (int type = 0; type < static_cast<int>(AllocationType::COUNT); type++) {
        AllocationProperties allocProperties(0, 1, static_cast<AllocationType>(type), {});
        AllocationData allocData{};
        allocData.flags.useSystemMemory = true;
        allocData.flags.requiresCpuAccess = false;

        hwHelper.setExtraAllocationData(allocData, allocProperties, *defaultHwInfo);

        if (defaultHwInfo->featureTable.flags.ftrLocalMemory &&
            (allocProperties.allocationType == AllocationType::COMMAND_BUFFER ||
             allocProperties.allocationType == AllocationType::RING_BUFFER ||
             allocProperties.allocationType == AllocationType::SEMAPHORE_BUFFER)) {
            EXPECT_FALSE(allocData.flags.useSystemMemory);
            EXPECT_TRUE(allocData.flags.requiresCpuAccess);
        } else {
            EXPECT_TRUE(allocData.flags.useSystemMemory);
            EXPECT_FALSE(allocData.flags.requiresCpuAccess);
        }
    }
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, GivenVariousValuesWhenAlignSlmSizeIsCalledThenCorrectValueIsReturned) {
    EXPECT_EQ(0u, HwHelperHw<FamilyType>::get().alignSlmSize(0));
    EXPECT_EQ(1024u, HwHelperHw<FamilyType>::get().alignSlmSize(1));
    EXPECT_EQ(1024u, HwHelperHw<FamilyType>::get().alignSlmSize(1024));
    EXPECT_EQ(2048u, HwHelperHw<FamilyType>::get().alignSlmSize(1025));
    EXPECT_EQ(2048u, HwHelperHw<FamilyType>::get().alignSlmSize(2048));
    EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(2049));
    EXPECT_EQ(4096u, HwHelperHw<FamilyType>::get().alignSlmSize(4096));
    EXPECT_EQ(8192u, HwHelperHw<FamilyType>::get().alignSlmSize(4097));
    EXPECT_EQ(8192u, HwHelperHw<FamilyType>::get().alignSlmSize(8192));
    EXPECT_EQ(16384u, HwHelperHw<FamilyType>::get().alignSlmSize(8193));
    EXPECT_EQ(16384u, HwHelperHw<FamilyType>::get().alignSlmSize(16384));
    EXPECT_EQ(32768u, HwHelperHw<FamilyType>::get().alignSlmSize(16385));
    EXPECT_EQ(32768u, HwHelperHw<FamilyType>::get().alignSlmSize(32768));
    EXPECT_EQ(65536u, HwHelperHw<FamilyType>::get().alignSlmSize(32769));
    EXPECT_EQ(65536u, HwHelperHw<FamilyType>::get().alignSlmSize(65536));
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenHwHelperWhenGettingThreadsPerEUConfigsThenCorrectConfigsAreReturned) {
    auto &helper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    EXPECT_NE(nullptr, &helper);

    auto &configs = helper.getThreadsPerEUConfigs();

    EXPECT_EQ(2U, configs.size());
    EXPECT_EQ(4U, configs[0]);
    EXPECT_EQ(8U, configs[1]);
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, WhenCheckingSipWAThenFalseIsReturned) {
    EXPECT_FALSE(HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily).isSipWANeeded(pDevice->getHardwareInfo()));
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenXeHPAndLaterPlatformWhenCheckAssignEngineRoundRobinSupportedThenReturnFalse) {
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    EXPECT_FALSE(hwHelper.isAssignEngineRoundRobinSupported(*defaultHwInfo));
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenHwHelperWhenCheckTimestampWaitSupportThenReturnFalse) {
    auto &helper = HwHelper::get(renderCoreFamily);
    EXPECT_FALSE(helper.isTimestampWaitSupportedForQueues());
    EXPECT_FALSE(helper.isTimestampWaitSupportedForEvents(*defaultHwInfo));
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenDisablePipeControlFlagIsEnabledWhenLocalMemoryIsEnabledThenReturnTrueAndProgramPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restore;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);

    HardwareInfo hardwareInfo = *defaultHwInfo;

    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    EXPECT_TRUE(MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hardwareInfo));

    constexpr size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, 0x1000, hardwareInfo);
    EXPECT_EQ(sizeof(PIPE_CONTROL), cmdStream.getUsed());
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenDisablePipeControlFlagIsEnabledWhenLocalMemoryIsDisabledThenReturnFalseAndDoNotProgramPipeControl) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);

    HardwareInfo hardwareInfo = *defaultHwInfo;

    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hardwareInfo));

    constexpr size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, 0x1000, hardwareInfo);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore, givenXeHpgCoreWhenCheckingIfEngineTypeRemappingIsRequiredThenReturnTrue) {
    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.isEngineTypeRemappingToHwSpecificRequired());
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore,
                  givenDebugFlagAndLocalMemoryIsNotAvailableWhenProgrammingPostSyncPipeControlThenExpectNotAddingWaPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restore;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);

    constexpr size_t bufferSize = 256u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    HardwareInfo hardwareInfo = *defaultHwInfo;
    hardwareInfo.featureTable.flags.ftrLocalMemory = false;

    PipeControlArgs args;
    uint64_t gpuAddress = 0xABC0;
    uint64_t immediateValue = 0x10;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(cmdStream,
                                                                               PostSyncMode::ImmediateData,
                                                                               gpuAddress,
                                                                               immediateValue,
                                                                               hardwareInfo,
                                                                               args);
    EXPECT_EQ(sizeof(PIPE_CONTROL), cmdStream.getUsed());

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    ASSERT_EQ(1u, hwParser.pipeControlList.size());

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*hwParser.pipeControlList.begin());
    EXPECT_EQ(gpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(immediateValue, pipeControl->getImmediateData());
}

XE_HPG_CORETEST_F(HwHelperTestXeHpgCore,
                  givenDebugFlagAndLocalMemoryIsAvailableWhenProgrammingPostSyncPipeControlThenExpectAddingWaPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restore;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);

    constexpr size_t bufferSize = 256u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    HardwareInfo hardwareInfo = *defaultHwInfo;
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    PipeControlArgs args;
    uint64_t gpuAddress = 0xABC0;
    uint64_t immediateValue = 0x10;
    MemorySynchronizationCommands<FamilyType>::addBarrierWithPostSyncOperation(cmdStream,
                                                                               PostSyncMode::ImmediateData,
                                                                               gpuAddress,
                                                                               immediateValue,
                                                                               hardwareInfo,
                                                                               args);
    EXPECT_EQ(sizeof(PIPE_CONTROL) * 2, cmdStream.getUsed());

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    ASSERT_EQ(2u, hwParser.pipeControlList.size());

    auto pipeControlItor = hwParser.pipeControlList.begin();
    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlItor);
    constexpr uint64_t zeroGpuAddress = 0;
    constexpr uint64_t zeroImmediateValue = 0;
    EXPECT_EQ(zeroGpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(zeroImmediateValue, pipeControl->getImmediateData());

    pipeControlItor++;
    pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlItor);
    EXPECT_EQ(gpuAddress, UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
    EXPECT_EQ(immediateValue, pipeControl->getImmediateData());
}
