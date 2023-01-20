/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "hw_cmds_xe_hpg_core_base.h"

using GfxCoreHelperTestXeHpgCore = GfxCoreHelperTest;
using ProductHelperTestXeHpgCore = Test<DeviceFixture>;

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenDifferentBufferSizesWhenEnableStatelessCompressionThenEveryBufferSizeIsSuitableForCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    const size_t sizesToCheck[] = {1, 128, 256, 1024, 2048};
    for (size_t size : sizesToCheck) {
        EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(size));
    }
}

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenDebugFlagWhenCheckingIfBufferIsSuitableThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    const size_t sizesToCheck[] = {1, 128, 256, 1024, 2048};

    for (int32_t debugFlag : {-1, 0, 1}) {
        DebugManager.flags.OverrideBufferSuitableForRenderCompression.set(debugFlag);

        for (size_t size : sizesToCheck) {
            if (debugFlag == 1) {
                EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(size));
            } else {
                EXPECT_FALSE(gfxCoreHelper.isBufferSizeSuitableForCompression(size));
            }
        }
    }
}

using ProductHelperTestXeHpgCore = Test<DeviceFixture>;

XE_HPG_CORETEST_F(ProductHelperTestXeHpgCore, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
    DebugManagerStateRestore restore;
    auto &productHelper = getHelper<ProductHelper>();

    HardwareInfo hwInfo = *defaultHwInfo;

    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
    productHelper.configureHardwareCustom(&hwInfo, nullptr);
    EXPECT_FALSE(hwInfo.capabilityTable.blitterOperationsSupported);

    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    productHelper.configureHardwareCustom(&hwInfo, nullptr);
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

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenAllocDataWhenSetExtraAllocationDataThenSetLocalMemForProperTypes) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    for (int type = 0; type < static_cast<int>(AllocationType::COUNT); type++) {
        AllocationProperties allocProperties(0, 1, static_cast<AllocationType>(type), {});
        AllocationData allocData{};
        allocData.flags.useSystemMemory = true;
        allocData.flags.requiresCpuAccess = false;

        gfxCoreHelper.setExtraAllocationData(allocData, allocProperties, *defaultHwInfo);

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

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, GivenVariousValuesWhenAlignSlmSizeIsCalledThenCorrectValueIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    EXPECT_EQ(0u, gfxCoreHelper.alignSlmSize(0));
    EXPECT_EQ(1024u, gfxCoreHelper.alignSlmSize(1));
    EXPECT_EQ(1024u, gfxCoreHelper.alignSlmSize(1024));
    EXPECT_EQ(2048u, gfxCoreHelper.alignSlmSize(1025));
    EXPECT_EQ(2048u, gfxCoreHelper.alignSlmSize(2048));
    EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(2049));
    EXPECT_EQ(4096u, gfxCoreHelper.alignSlmSize(4096));
    EXPECT_EQ(8192u, gfxCoreHelper.alignSlmSize(4097));
    EXPECT_EQ(8192u, gfxCoreHelper.alignSlmSize(8192));
    EXPECT_EQ(16384u, gfxCoreHelper.alignSlmSize(8193));
    EXPECT_EQ(16384u, gfxCoreHelper.alignSlmSize(16384));
    EXPECT_EQ(32768u, gfxCoreHelper.alignSlmSize(16385));
    EXPECT_EQ(32768u, gfxCoreHelper.alignSlmSize(32768));
    EXPECT_EQ(65536u, gfxCoreHelper.alignSlmSize(32769));
    EXPECT_EQ(65536u, gfxCoreHelper.alignSlmSize(65536));
}

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenGfxCoreHelperWhenGettingThreadsPerEUConfigsThenCorrectConfigsAreReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    auto &configs = gfxCoreHelper.getThreadsPerEUConfigs();

    EXPECT_EQ(2U, configs.size());
    EXPECT_EQ(4U, configs[0]);
    EXPECT_EQ(8U, configs[1]);
}

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, WhenCheckingSipWAThenFalseIsReturned) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    EXPECT_FALSE(gfxCoreHelper.isSipWANeeded(pDevice->getHardwareInfo()));
}

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenXeHPAndLaterPlatformWhenCheckAssignEngineRoundRobinSupportedThenReturnFalse) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isAssignEngineRoundRobinSupported(*defaultHwInfo));
}

XE_HPG_CORETEST_F(ProductHelperTestXeHpgCore, givenProductHelperWhenCheckTimestampWaitSupportForEventsThenReturnFalse) {
    auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isTimestampWaitSupportedForEvents());
}

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenGfxCoreHelperWhenCheckTimestampWaitSupportForQueuesThenReturnFalse) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isTimestampWaitSupportedForQueues());
}

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenDisablePipeControlFlagIsEnabledWhenLocalMemoryIsEnabledThenReturnTrueAndProgramPipeControl) {
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

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenDisablePipeControlFlagIsEnabledWhenLocalMemoryIsDisabledThenReturnFalseAndDoNotProgramPipeControl) {
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

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenXeHpgCoreWhenCheckingIfEngineTypeRemappingIsRequiredThenReturnTrue) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.isEngineTypeRemappingToHwSpecificRequired());
}

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore,
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

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore,
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

XE_HPG_CORETEST_F(GfxCoreHelperTestXeHpgCore, givenGfxCoreHelperWhenCallCopyThroughLockedPtrEnabledThenReturnFalse) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_FALSE(gfxCoreHelper.copyThroughLockedPtrEnabled(*defaultHwInfo));
}