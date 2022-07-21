/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using HwHelperTestXE_HP_CORE = HwHelperTest;

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenGenHelperWhenKernelArgumentIsNotPureStatefulThenRequireNonAuxMode) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto isPureStateful : {false, true}) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;

        EXPECT_EQ(!argAsPtr.isPureStateful(), clHwHelper.requiresNonAuxMode(argAsPtr, *defaultHwInfo));
    }
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenGenHelperWhenEnableStatelessCompressionThenDontRequireNonAuxMode) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto isPureStateful : {false, true}) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;
        EXPECT_FALSE(clHwHelper.requiresNonAuxMode(argAsPtr, *defaultHwInfo));
    }
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenXE_HP_COREThenAuxTranslationIsRequired) {
    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);

    for (auto isPureStateful : {false, true}) {
        KernelInfo kernelInfo{};
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = !isPureStateful;
        EXPECT_EQ(!isPureStateful, clHwHelper.requiresAuxResolves(kernelInfo, *defaultHwInfo));
    }
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenXE_HP_COREWhenEnableStatelessCompressionThenAuxTranslationIsNotRequired) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &clHwHelper = ClHwHelper::get(renderCoreFamily);
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clHwHelper.requiresAuxResolves(kernelInfo, *defaultHwInfo));
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenDifferentBufferSizesWhenEnableStatelessCompressionThenEveryBufferSizeIsSuitableForCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &helper = HwHelper::get(renderCoreFamily);

    const size_t sizesToCheck[] = {1, 128, 256, 1024, 2048};
    for (size_t size : sizesToCheck) {
        EXPECT_TRUE(helper.isBufferSizeSuitableForCompression(size, *defaultHwInfo));
    }
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenStatelessCompressionEnabledWhenSetExtraAllocationDataThenDontRequireCpuAccessNorMakeResourceLocableForCompressedAllocations) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    for (auto allocType : {AllocationType::CONSTANT_SURFACE, AllocationType::GLOBAL_SURFACE, AllocationType::PRINTF_SURFACE}) {
        AllocationData allocData;
        AllocationProperties allocProperties(mockRootDeviceIndex, true, allocType, mockDeviceBitfield);

        hwHelper.setExtraAllocationData(allocData, allocProperties, hwInfo);
        EXPECT_FALSE(allocData.flags.requiresCpuAccess);
        EXPECT_FALSE(allocData.storageInfo.isLockable);
    }
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_C,
        REVISION_D,
        CommonConstants::invalidStepping,
    };

    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (hardwareInfo.platform.eProductFamily == IGFX_XE_HP_SDV) {
            if (stepping == REVISION_A0) {
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo));
                EXPECT_TRUE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo));
            } else if (stepping == REVISION_A1) {
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo));
            } else if (stepping == REVISION_C || stepping == REVISION_D) { //undefined
                EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
            }
        } else {
            EXPECT_FALSE(hwHelper.isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo));
        }
    }
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenRevisionEnumThenProperMaxThreadsForWorkgroupIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    EXPECT_EQ(64u, hwInfoConfig.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));

    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hardwareInfo);
    uint32_t numThreadsPerEU = hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount;
    EXPECT_EQ(64u * numThreadsPerEU, hwInfoConfig.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenDisablePipeControlFlagIsDefaultWhenLocalMemoryIsEnabledThenReturnFalseAndDoNotProgramPipeControl) {
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hardwareInfo));

    constexpr size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, 0x1000, hardwareInfo);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenDisablePipeControlFlagIsDisabledWhenLocalMemoryIsEnabledThenReturnFalseAndDoNotProgramPipeControl) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(0);

    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hardwareInfo));

    constexpr size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, 0x1000, hardwareInfo);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenDisablePipeControlFlagIsEnabledWhenLocalMemoryIsEnabledThenReturnTrueAndProgramPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    DebugManagerStateRestore restore;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);

    hardwareInfo.featureTable.flags.ftrLocalMemory = true;
    EXPECT_TRUE(MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hardwareInfo));

    constexpr size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, 0x1000, hardwareInfo);
    EXPECT_EQ(sizeof(PIPE_CONTROL), cmdStream.getUsed());
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenDisablePipeControlFlagIsEnabledWhenLocalMemoryIsDisabledThenReturnTrueAndDoNotProgramPipeControl) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisablePipeControlPrecedingPostSyncCommand.set(1);

    hardwareInfo.featureTable.flags.ftrLocalMemory = false;
    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hardwareInfo));

    constexpr size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, 0x1000, hardwareInfo);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenXeHPAndLaterPlatformWhenCheckAssignEngineRoundRobinSupportedThenReturnFalse) {
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    EXPECT_FALSE(hwHelper.isAssignEngineRoundRobinSupported(*defaultHwInfo));
}

using HwInfoConfigTestXE_HP_CORE = ::testing::Test;

XE_HP_CORE_TEST_F(HwInfoConfigTestXE_HP_CORE, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
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

XE_HP_CORE_TEST_F(HwInfoConfigTestXE_HP_CORE, givenMultitileConfigWhenConfiguringHwInfoThenEnableBlitter) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);

    HardwareInfo hwInfo = *defaultHwInfo;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(true, hwInfo.capabilityTable.blitterOperationsSupported);
    }
}

using LriHelperTestsXE_HP_CORE = ::testing::Test;

XE_HP_CORE_TEST_F(LriHelperTestsXE_HP_CORE, whenProgrammingLriCommandThenExpectMmioRemapEnableCorrectlySet) {
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

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, GivenVariousValuesWhenAlignSlmSizeIsCalledThenCorrectValueIsReturned) {
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

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, givenHwHelperWhenGettingThreadsPerEUConfigsThenCorrectConfigsAreReturned) {
    auto &helper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    EXPECT_NE(nullptr, &helper);

    auto &configs = helper.getThreadsPerEUConfigs();

    EXPECT_EQ(2U, configs.size());
    EXPECT_EQ(4U, configs[0]);
    EXPECT_EQ(8U, configs[1]);
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(12, 5, 1), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE, whenGettingDefaultRevisionThenB0IsReturned) {
    EXPECT_EQ(HwInfoConfigHw<IGFX_XE_HP_SDV>::get()->getHwRevIdFromStepping(REVISION_B, *defaultHwInfo), HwHelperHw<FamilyType>::get().getDefaultRevisionId(*defaultHwInfo));
}

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE,
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

XE_HP_CORE_TEST_F(HwHelperTestXE_HP_CORE,
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

using HwHelperTestXeHpCoreAndLater = HwHelperTest;
HWTEST2_F(HwHelperTestXeHpCoreAndLater, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue, IsAtLeastXeHpCore) {
    cl_device_feature_capabilities_intel expectedCapabilities = CL_DEVICE_FEATURE_FLAG_DPAS_INTEL | CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
    EXPECT_EQ(expectedCapabilities, ClHwHelper::get(renderCoreFamily).getSupportedDeviceFeatureCapabilities(hardwareInfo));
}
