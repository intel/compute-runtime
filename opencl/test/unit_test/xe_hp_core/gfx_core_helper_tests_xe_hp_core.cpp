/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_gfx_core_helper.h"

using ClGfxCoreHelperTestsXeHpCore = Test<ClDeviceFixture>;

XE_HP_CORE_TEST_F(ClGfxCoreHelperTestsXeHpCore, givenGenHelperWhenKernelArgumentIsNotPureStatefulThenRequireNonAuxMode) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto isPureStateful : {false, true}) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;

        EXPECT_EQ(!argAsPtr.isPureStateful(), clGfxCoreHelper.requiresNonAuxMode(argAsPtr, getRootDeviceEnvironment()));
    }
}

XE_HP_CORE_TEST_F(ClGfxCoreHelperTestsXeHpCore, givenGenHelperWhenEnableStatelessCompressionThenDontRequireNonAuxMode) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto isPureStateful : {false, true}) {
        ArgDescPointer argAsPtr{};
        argAsPtr.accessedUsingStatelessAddressingMode = !isPureStateful;
        EXPECT_FALSE(clGfxCoreHelper.requiresNonAuxMode(argAsPtr, getRootDeviceEnvironment()));
    }
}

XE_HP_CORE_TEST_F(ClGfxCoreHelperTestsXeHpCore, givenXE_HP_COREThenAuxTranslationIsRequired) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();

    for (auto isPureStateful : {false, true}) {
        KernelInfo kernelInfo{};
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
        kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = !isPureStateful;
        EXPECT_EQ(!isPureStateful, clGfxCoreHelper.requiresAuxResolves(kernelInfo, getRootDeviceEnvironment()));
    }
}

XE_HP_CORE_TEST_F(ClGfxCoreHelperTestsXeHpCore, givenXE_HP_COREWhenEnableStatelessCompressionThenAuxTranslationIsNotRequired) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    KernelInfo kernelInfo{};

    EXPECT_FALSE(clGfxCoreHelper.requiresAuxResolves(kernelInfo, getRootDeviceEnvironment()));
}

XE_HP_CORE_TEST_F(ClGfxCoreHelperTestsXeHpCore, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    EXPECT_EQ(ClGfxCoreHelperMock::makeDeviceIpVersion(12, 5, 1), clGfxCoreHelper.getDeviceIpVersion(*defaultHwInfo));
}

using GfxCoreHelperTestXE_HP_CORE = GfxCoreHelperTest;

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenDifferentBufferSizesWhenEnableStatelessCompressionThenEveryBufferSizeIsSuitableForCompression) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    const size_t sizesToCheck[] = {1, 128, 256, 1024, 2048};
    for (size_t size : sizesToCheck) {
        EXPECT_TRUE(gfxCoreHelper.isBufferSizeSuitableForCompression(size, *defaultHwInfo));
    }
}

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenStatelessCompressionEnabledWhenSetExtraAllocationDataThenDontRequireCpuAccessNorMakeResourceLocableForCompressedAllocations) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableStatelessCompression.set(1);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    for (auto allocType : {AllocationType::CONSTANT_SURFACE, AllocationType::GLOBAL_SURFACE, AllocationType::PRINTF_SURFACE}) {
        AllocationData allocData;
        AllocationProperties allocProperties(mockRootDeviceIndex, true, allocType, mockDeviceBitfield);

        gfxCoreHelper.setExtraAllocationData(allocData, allocProperties, hwInfo);
        EXPECT_FALSE(allocData.flags.requiresCpuAccess);
        EXPECT_FALSE(allocData.storageInfo.isLockable);
    }
}

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenRevisionEnumAndPlatformFamilyTypeThenProperValueForIsWorkaroundRequiredIsReturned) {
    uint32_t steppings[] = {
        REVISION_A0,
        REVISION_A1,
        REVISION_C,
        REVISION_D,
        CommonConstants::invalidStepping,
    };

    const auto &productHelper = getHelper<ProductHelper>();

    for (auto stepping : steppings) {
        hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(stepping, hardwareInfo);

        if (hardwareInfo.platform.eProductFamily == IGFX_XE_HP_SDV) {
            if (stepping == REVISION_A0) {
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_B, hardwareInfo, productHelper));
                EXPECT_TRUE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, productHelper));
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_B, REVISION_A0, hardwareInfo, productHelper));
            } else if (stepping == REVISION_A1) {
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_A1, hardwareInfo, productHelper));
            } else if (stepping == REVISION_C || stepping == REVISION_D) { // undefined
                EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
            }
        } else {
            EXPECT_FALSE(GfxCoreHelper::isWorkaroundRequired(REVISION_A0, REVISION_D, hardwareInfo, productHelper));
        }
    }
}

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenRevisionEnumThenProperMaxThreadsForWorkgroupIsReturned) {
    const auto &productHelper = *ProductHelper::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    EXPECT_EQ(64u, productHelper.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));

    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hardwareInfo);
    uint32_t numThreadsPerEU = hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount;
    EXPECT_EQ(64u * numThreadsPerEU, productHelper.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));
}

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenDisablePipeControlFlagIsDefaultWhenLocalMemoryIsEnabledThenReturnFalseAndDoNotProgramPipeControl) {
    hardwareInfo.featureTable.flags.ftrLocalMemory = true;

    EXPECT_FALSE(MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(hardwareInfo));

    constexpr size_t bufferSize = 128u;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    MemorySynchronizationCommands<FamilyType>::addBarrierWa(cmdStream, 0x1000, hardwareInfo);
    EXPECT_EQ(0u, cmdStream.getUsed());
}

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenDisablePipeControlFlagIsDisabledWhenLocalMemoryIsEnabledThenReturnFalseAndDoNotProgramPipeControl) {
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

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenDisablePipeControlFlagIsEnabledWhenLocalMemoryIsEnabledThenReturnTrueAndProgramPipeControl) {
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

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenDisablePipeControlFlagIsEnabledWhenLocalMemoryIsDisabledThenReturnTrueAndDoNotProgramPipeControl) {
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

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenXeHPAndLaterPlatformWhenCheckAssignEngineRoundRobinSupportedThenReturnFalse) {
    auto &gfxCoreHelper = GfxCoreHelperHw<FamilyType>::get();
    auto &productHelper = getHelper<ProductHelper>();

    EXPECT_FALSE(productHelper.isAssignEngineRoundRobinSupported());
}

using ProductHelperTestXE_HP_CORE = Test<DeviceFixture>;

XE_HP_CORE_TEST_F(ProductHelperTestXE_HP_CORE, givenDebugVariableSetWhenConfigureIsCalledThenSetupBlitterOperationsSupportedFlag) {
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

XE_HP_CORE_TEST_F(ProductHelperTestXE_HP_CORE, givenMultitileConfigWhenConfiguringHwInfoThenEnableBlitter) {
    auto &productHelper = getHelper<ProductHelper>();

    HardwareInfo hwInfo = *defaultHwInfo;

    for (uint32_t tileCount = 0; tileCount <= 4; tileCount++) {
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = tileCount;
        productHelper.configureHardwareCustom(&hwInfo, nullptr);

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

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, GivenVariousValuesWhenAlignSlmSizeIsCalledThenCorrectValueIsReturned) {
    EXPECT_EQ(0u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(0));
    EXPECT_EQ(1024u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(1));
    EXPECT_EQ(1024u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(1024));
    EXPECT_EQ(2048u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(1025));
    EXPECT_EQ(2048u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(2048));
    EXPECT_EQ(4096u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(2049));
    EXPECT_EQ(4096u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(4096));
    EXPECT_EQ(8192u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(4097));
    EXPECT_EQ(8192u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(8192));
    EXPECT_EQ(16384u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(8193));
    EXPECT_EQ(16384u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(16384));
    EXPECT_EQ(32768u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(16385));
    EXPECT_EQ(32768u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(32768));
    EXPECT_EQ(65536u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(32769));
    EXPECT_EQ(65536u, GfxCoreHelperHw<FamilyType>::get().alignSlmSize(65536));
}

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE, givenGfxCoreHelperWhenGettingThreadsPerEUConfigsThenCorrectConfigsAreReturned) {
    auto &helper = pDevice->getGfxCoreHelper();
    auto &configs = helper.getThreadsPerEUConfigs();

    EXPECT_EQ(2U, configs.size());
    EXPECT_EQ(4U, configs[0]);
    EXPECT_EQ(8U, configs[1]);
}

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE,
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

XE_HP_CORE_TEST_F(GfxCoreHelperTestXE_HP_CORE,
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
