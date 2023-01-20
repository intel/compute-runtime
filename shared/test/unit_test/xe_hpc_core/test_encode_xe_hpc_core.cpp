/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/definitions/mi_flush_args.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/helpers/state_base_address_tests.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "hw_cmds_xe_hpc_core_base.h"

using namespace NEO;

HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterEncodeMiFlushDWTest, whenMiFlushDwIsProgrammedThenSetFlushCcsAndLlc, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTests, whenEncodeMemoryPrefetchCalledThenDoNothing, IGFX_XE_HPC_CORE);

using CommandEncodeXeHpcCoreTest = ::testing::Test;

XE_HPC_CORETEST_F(CommandEncodeXeHpcCoreTest, whenMiFlushDwIsProgrammedThenSetAndFlushLlcWithoutCcs) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    MiFlushArgs args;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<FamilyType>::programMiFlushDw(linearStream, 0x1230000, 456, args, *defaultHwInfo);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);
    EXPECT_EQ(0u, miFlushDwCmd->getFlushCcs());
    EXPECT_EQ(1u, miFlushDwCmd->getFlushLlc());
}

XE_HPC_CORETEST_F(CommandEncodeXeHpcCoreTest, givenOffsetWhenProgrammingStatePrefetchThenSetCorrectGpuVa) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 0b0011'1000; // [3:5] - BaseDie != A0

    uint8_t buffer[sizeof(STATE_PREFETCH) * 4] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    constexpr uint64_t gpuVa = 0x100000;
    constexpr uint32_t gpuVaOffset = 0x10000;

    const GraphicsAllocation allocation(0, AllocationType::BUFFER, nullptr, gpuVa, 0, 4096, MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);

    memset(buffer, 0, sizeof(buffer));
    LinearStream linearStream(buffer, sizeof(buffer));

    uint32_t expectedCmdsCount = 3;
    uint32_t alignedSize = MemoryConstants::pageSize64k * expectedCmdsCount;

    EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, alignedSize, gpuVaOffset, hwInfo);
    EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, linearStream.getUsed());

    for (uint32_t i = 0; i < expectedCmdsCount; i++) {
        uint64_t expectedVa = gpuVa + gpuVaOffset + (i * MemoryConstants::pageSize64k);
        EXPECT_EQ(expectedVa, statePrefetchCmd[i].getAddress());
    }
}

XE_HPC_CORETEST_F(CommandEncodeXeHpcCoreTest, givenDebugVariableSetwhenProgramingStatePrefetchThenSetCorrectFields) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 0b0011'1000; // [3:5] - BaseDie != A0

    uint8_t buffer[sizeof(STATE_PREFETCH) * 4] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    constexpr uint64_t gpuVa = 0x100000;
    constexpr uint32_t mocsIndexForL3 = (2 << 1);
    constexpr size_t numCachelines = 3;

    const GraphicsAllocation allocation(0, AllocationType::BUFFER, nullptr, gpuVa, 0, 4096, MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);

    constexpr std::array<uint32_t, 7> expectedSizes = {{
        MemoryConstants::cacheLineSize - 1,
        MemoryConstants::cacheLineSize,
        MemoryConstants::cacheLineSize + 1,
        MemoryConstants::cacheLineSize * numCachelines,
        MemoryConstants::pageSize64k - 1,
        MemoryConstants::pageSize64k,
        (MemoryConstants::pageSize64k * 2) + 1,
    }};

    for (auto expectedSize : expectedSizes) {
        memset(buffer, 0, sizeof(buffer));
        LinearStream linearStream(buffer, sizeof(buffer));

        uint32_t alignedSize = alignUp(expectedSize, MemoryConstants::pageSize64k);
        uint32_t expectedCmdsCount = std::max((alignedSize / static_cast<uint32_t>(MemoryConstants::pageSize64k)), 1u);

        EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(expectedSize, hwInfo));

        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, expectedSize, 0, hwInfo);
        EXPECT_EQ(sizeof(STATE_PREFETCH) * expectedCmdsCount, linearStream.getUsed());

        for (uint32_t i = 0; i < expectedCmdsCount; i++) {
            uint32_t programmedSize = (statePrefetchCmd[i].getPrefetchSize() + 1) * MemoryConstants::cacheLineSize;

            EXPECT_EQ(statePrefetchCmd[i].getAddress(), gpuVa + (i * MemoryConstants::pageSize64k));
            EXPECT_FALSE(statePrefetchCmd[i].getKernelInstructionPrefetch());
            EXPECT_FALSE(statePrefetchCmd[i].getParserStall());
            EXPECT_EQ(mocsIndexForL3, statePrefetchCmd[i].getMemoryObjectControlState());

            if (programmedSize > expectedSize) {
                // cacheline alignemnt
                EXPECT_TRUE((programmedSize - expectedSize) < MemoryConstants::cacheLineSize);
                expectedSize = 0;
            } else {
                expectedSize -= programmedSize;
            }
        }
        EXPECT_EQ(0u, expectedSize);
    }
}

XE_HPC_CORETEST_F(CommandEncodeXeHpcCoreTest, givenIsaAllocationWhenProgrammingPrefetchThenSetKernelInstructionPrefetchBit) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 0b0011'1000; // [3:5] - BaseDie != A0

    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};
    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    EXPECT_EQ(sizeof(STATE_PREFETCH), EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(1, hwInfo));

    AllocationType isaTypes[] = {AllocationType::KERNEL_ISA, AllocationType::KERNEL_ISA_INTERNAL};

    for (uint32_t i = 0; i < 2; i++) {
        memset(buffer, 0, sizeof(STATE_PREFETCH));
        LinearStream linearStream(buffer, sizeof(buffer));

        const GraphicsAllocation allocation(0, isaTypes[i],
                                            nullptr, 1234, 0, 4096, MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);

        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, hwInfo);
        EXPECT_EQ(sizeof(STATE_PREFETCH), linearStream.getUsed());

        EXPECT_TRUE(statePrefetchCmd->getKernelInstructionPrefetch());
    }
}

XE_HPC_CORETEST_F(CommandEncodeXeHpcCoreTest, givenDebugFlagSetWhenProgramPrefetchCalledThenDoPrefetchIfSetToOne) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    DebugManagerStateRestore restore;

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 0b0011'1000; // [3:5] - BaseDie != A0

    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};

    AllocationType isaTypes[] = {AllocationType::KERNEL_ISA, AllocationType::KERNEL_ISA_INTERNAL};

    for (uint32_t i = 0; i < 2; i++) {
        memset(buffer, 0, sizeof(STATE_PREFETCH));
        const GraphicsAllocation allocation(0, isaTypes[i],
                                            nullptr, 1234, 0, 4096, MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);

        LinearStream linearStream(buffer, sizeof(buffer));

        DebugManager.flags.EnableMemoryPrefetch.set(0);
        EXPECT_EQ(0u, EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(100, hwInfo));
        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 100, 0, hwInfo);
        EXPECT_EQ(0u, linearStream.getUsed());

        DebugManager.flags.EnableMemoryPrefetch.set(1);
        EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, hwInfo);
        EXPECT_EQ(sizeof(STATE_PREFETCH), linearStream.getUsed());
        auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);
        EXPECT_TRUE(statePrefetchCmd->getKernelInstructionPrefetch());
    }
}

XE_HPC_CORETEST_F(CommandEncodeXeHpcCoreTest, givenDebugFlagSetWhenProgrammingPrefetchThenSetParserStall) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;

    DebugManagerStateRestore restore;
    DebugManager.flags.ForceCsStallForStatePrefetch.set(1);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 0b0010'1000; // [3:5] - BaseDie != A0

    const GraphicsAllocation allocation(0, AllocationType::BUFFER,
                                        nullptr, 1234, 0, 4096, MemoryPool::LocalMemory, MemoryManager::maxOsContextCount);
    uint8_t buffer[sizeof(STATE_PREFETCH)] = {};

    LinearStream linearStream(buffer, sizeof(buffer));

    EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 123, 0, hwInfo);

    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(buffer);

    EXPECT_TRUE(statePrefetchCmd->getParserStall());
}

XE_HPC_CORETEST_F(CommandEncodeXeHpcCoreTest, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using EU_THREAD_SCHEDULING_MODE_OVERRIDE = typename STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE;
    uint8_t buffer[64]{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment, nullptr);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(0u, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
    EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT, pScm->getEuThreadSchedulingModeOverride());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 0;
    properties.threadArbitrationPolicy.value = ThreadArbitrationPolicy::RoundRobin;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(0u, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
    EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT, pScm->getEuThreadSchedulingModeOverride());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.isDirty = true;
    properties.threadArbitrationPolicy.isDirty = true;
    properties.largeGrfMode.isDirty = true;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask |
                        FamilyType::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, pScm->getForceNonCoherent());
    EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, pScm->getEuThreadSchedulingModeOverride());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

XE_HPC_CORETEST_F(CommandEncodeXeHpcCoreTest, whenAdjustComputeModeIsCalledThenCorrectPolicyIsProgrammed) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using EU_THREAD_SCHEDULING_MODE_OVERRIDE = typename STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE;

    uint8_t buffer[64]{};
    StreamProperties properties{};

    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    properties.stateComputeMode.setProperties(false, 0, ThreadArbitrationPolicy::AgeBased, PreemptionMode::Disabled, *executionEnvironment.rootDeviceEnvironments[0]);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties.stateComputeMode, rootDeviceEnvironment, nullptr);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    if (productHelper.isThreadArbitrationPolicyReportedWithScm()) {
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, pScm->getEuThreadSchedulingModeOverride());
    } else {
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT, pScm->getEuThreadSchedulingModeOverride());
    }

    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    properties.stateComputeMode.setProperties(false, 0, ThreadArbitrationPolicy::RoundRobin, PreemptionMode::Disabled, *executionEnvironment.rootDeviceEnvironments[0]);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties.stateComputeMode, rootDeviceEnvironment, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    if (productHelper.isThreadArbitrationPolicyReportedWithScm()) {
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, pScm->getEuThreadSchedulingModeOverride());
    } else {
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT, pScm->getEuThreadSchedulingModeOverride());
    }

    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    properties.stateComputeMode.setProperties(false, 0, ThreadArbitrationPolicy::RoundRobinAfterDependency, PreemptionMode::Disabled, *executionEnvironment.rootDeviceEnvironments[0]);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties.stateComputeMode, rootDeviceEnvironment, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    if (productHelper.isThreadArbitrationPolicyReportedWithScm()) {
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, pScm->getEuThreadSchedulingModeOverride());
    } else {
        EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT, pScm->getEuThreadSchedulingModeOverride());
    }

    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    properties.stateComputeMode.setProperties(false, 0, ThreadArbitrationPolicy::NotPresent, PreemptionMode::Disabled, *executionEnvironment.rootDeviceEnvironments[0]);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties.stateComputeMode, rootDeviceEnvironment, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT, pScm->getEuThreadSchedulingModeOverride());
}

using EncodeKernelXeHpcCoreTest = Test<CommandEncodeStatesFixture>;

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenNoFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenDoNotGenerateFenceCommands) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    DebugManagerStateRestore restore;
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0u;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenGenerateFenceCommands) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    DebugManagerStateRestore restore;
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0u;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenDefaultSettingForFenceAsPostSyncOperationInComputeWalkerWhenEnqueueKernelIsCalledThenDoNotGenerateFenceCommands) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    DebugManagerStateRestore restore;
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0u;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isKernelUsingSystemAllocation = true;
    dispatchArgs.isHostScopeSignalEvent = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenDefaultSettingForFenceWhenKernelUsesSystemMemoryFlagTrueAndNoHostSignalEventThenNotUseSystemFence) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    DebugManagerStateRestore restore;
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);

    unsigned short pvcRevB = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    VariableBackup<unsigned short> hwRevId(&hwInfo.platform.usRevId, pvcRevB);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0u;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isKernelUsingSystemAllocation = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        cmdContainer->getCommandStream()->getCpuBase(),
        cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenDefaultSettingForFenceWhenEventHostSignalScopeFlagTrueAndNoSystemMemoryThenNotUseSystemFence) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    DebugManagerStateRestore restore;
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);

    unsigned short pvcRevB = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    VariableBackup<unsigned short> hwRevId(&hwInfo.platform.usRevId, pvcRevB);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0u;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.isHostScopeSignalEvent = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        cmdContainer->getCommandStream()->getCpuBase(),
        cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenDefaultSettingForFenceWhenKernelUsesSystemMemoryAndHostSignalEventFlagTrueThenUseSystemFence) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    DebugManagerStateRestore restore;
    DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);

    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);

    unsigned short pvcRevB = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    VariableBackup<unsigned short> hwRevId(&hwInfo.platform.usRevId, pvcRevB);

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->getCrossThreadDataSizeResult = 0;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.isKernelUsingSystemAllocation = true;
    dispatchArgs.isHostScopeSignalEvent = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenCleanHeapsAndSlmNotChangedAndUncachedMocsRequestedThenSBAIsProgrammedAndMocsAreSet) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    auto gmmHelper = cmdContainer->getDevice()->getGmmHelper();
    EXPECT_EQ(cmdSba->getStatelessDataPortAccessMemoryObjectControlState(),
              (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED)));
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenDispatchSizeSmallerOrEqualToAvailableThreadCountWhenAdjustInterfaceDescriptorDataIsCalledThenThreadGroupDispatchSizeIsCorrectlySet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.gtSystemInfo.EUCount = 2u;

    const uint32_t numGrf = GrfConfig::LargeGrfNumber;
    INTERFACE_DESCRIPTOR_DATA iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);

    for (const auto threadGroupCount : {1u, 2u}) {
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);

        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
    }
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenMultipleTilesAndImplicitScalingWhenAdjustInterfaceDescriptorDataIsCalledThenThreadGroupDispatchSizeIsCorrectlySet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(0);
    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
    hwInfo.gtSystemInfo.EUCount = 32;
    hwInfo.gtSystemInfo.DualSubSliceCount = hwInfo.gtSystemInfo.MaxDualSubSlicesSupported;
    INTERFACE_DESCRIPTOR_DATA iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    const uint32_t numGrf = GrfConfig::DefaultGrfNumber;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const uint32_t threadGroupCount = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, numGrf) / 32u;
    iddArg.setNumberOfThreadsInGpgpuThreadGroup(64u);

    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);
    ASSERT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());

    DebugManager.flags.EnableWalkerPartition.set(1);
    pDevice->numSubDevices = 2;
    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenNumberOfThreadsInThreadGroupWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsCorrectlySet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    INTERFACE_DESCRIPTOR_DATA iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    const uint32_t threadGroupCount = 512u;
    const uint32_t numGrf = GrfConfig::DefaultGrfNumber;
    std::array<std::pair<uint32_t, uint32_t>, 3> testParams = {{{16u, INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8},
                                                                {32u, INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4},
                                                                {64u, INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2}}};

    for (const auto &[numberOfThreadsInThreadGroup, expectedThreadGroupDispatchSize] : testParams) {
        iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInThreadGroup);

        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);

        EXPECT_EQ(expectedThreadGroupDispatchSize, iddArg.getThreadGroupDispatchSize());
    }
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenDifferentNumGrfWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsCorrectlySet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    INTERFACE_DESCRIPTOR_DATA iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    const uint32_t numberOfThreadsInThreadGroup = 1u;

    {
        const uint32_t numGrf = GrfConfig::DefaultGrfNumber;
        const uint32_t threadGroupCount = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, numGrf);
        iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInThreadGroup);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);
        ASSERT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
    }

    {
        const uint32_t numGrf = GrfConfig::LargeGrfNumber;
        const uint32_t threadGroupCount = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, numGrf);
        iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInThreadGroup);
        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
    }
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenNumberOfThreadsInThreadGroupAndDebugFlagDisabledWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsDefault) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    DebugManagerStateRestore restorer;
    DebugManager.flags.AdjustThreadGroupDispatchSize.set(0);
    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    INTERFACE_DESCRIPTOR_DATA iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    const uint32_t threadGroupCount = 512u;
    const uint32_t numGrf = GrfConfig::DefaultGrfNumber;
    std::array<std::pair<uint32_t, uint32_t>, 3> testParams = {{{16u, INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1},
                                                                {32u, INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1},
                                                                {64u, INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1}}};

    for (const auto &[numberOfThreadsInThreadGroup, expectedThreadGroupDispatchSize] : testParams) {
        iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInThreadGroup);

        EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);

        EXPECT_EQ(expectedThreadGroupDispatchSize, iddArg.getThreadGroupDispatchSize());
    }
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenIndivisibleDispatchSizeWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsCorrectlySet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    const uint32_t threadGroupCount = 343u;
    const uint32_t numGrf = GrfConfig::DefaultGrfNumber;
    INTERFACE_DESCRIPTOR_DATA iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    iddArg.setNumberOfThreadsInGpgpuThreadGroup(18u);

    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);

    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
}

XE_HPC_CORETEST_F(EncodeKernelXeHpcCoreTest, givenThreadGroupCountZeroWhenCallingAdjustInterfaceDescriptorDataThenThreadGroupDispatchSizeIsSetToDefault) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    const auto &productHelper = *ProductHelper::get(productFamily);
    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

    const uint32_t threadGroupCount = 0u;
    const uint32_t numGrf = GrfConfig::DefaultGrfNumber;
    INTERFACE_DESCRIPTOR_DATA iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    iddArg.setNumberOfThreadsInGpgpuThreadGroup(1u);

    EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, *pDevice, hwInfo, threadGroupCount, numGrf);

    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
}

using XeHpcSbaTest = SbaTest;

XE_HPC_CORETEST_F(XeHpcSbaTest, givenSpecificProductFamilyWhenAppendingSbaThenProgramWtL1CachePolicy) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                                  // generalStateBase
        0,                                                  // indirectObjectHeapBaseAddress
        0,                                                  // instructionHeapBaseAddress
        0,                                                  // globalHeapsBaseAddress
        0,                                                  // surfaceStateBaseAddress
        &sbaCmd,                                            // stateBaseAddressCmd
        nullptr,                                            // dsh
        nullptr,                                            // ioh
        &ssh,                                               // ssh
        pDevice->getRootDeviceEnvironment().getGmmHelper(), // gmmHelper
        nullptr,                                            // hwInfo
        0,                                                  // statelessMocsIndex
        MemoryCompressionState::NotApplicable,              // memoryCompressionState
        false,                                              // setInstructionStateBaseAddress
        true,                                               // setGeneralStateBaseAddress
        false,                                              // useGlobalHeapsBaseAddress
        false,                                              // isMultiOsContextCapable
        false,                                              // useGlobalAtomics
        false,                                              // areMultipleSubDevicesInContext
        false                                               // overrideSurfaceStateBaseAddress
    };
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, sbaCmd.getL1CachePolicyL1CacheControl());
}

XE_HPC_CORETEST_F(XeHpcSbaTest, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceStatelessL1CachingPolicy.set(0u);
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                                  // generalStateBase
        0,                                                  // indirectObjectHeapBaseAddress
        0,                                                  // instructionHeapBaseAddress
        0,                                                  // globalHeapsBaseAddress
        0,                                                  // surfaceStateBaseAddress
        &sbaCmd,                                            // stateBaseAddressCmd
        nullptr,                                            // dsh
        nullptr,                                            // ioh
        &ssh,                                               // ssh
        pDevice->getRootDeviceEnvironment().getGmmHelper(), // gmmHelper
        nullptr,                                            // hwInfo
        0,                                                  // statelessMocsIndex
        MemoryCompressionState::NotApplicable,              // memoryCompressionState
        false,                                              // setInstructionStateBaseAddress
        true,                                               // setGeneralStateBaseAddress
        false,                                              // useGlobalHeapsBaseAddress
        false,                                              // isMultiOsContextCapable
        false,                                              // useGlobalAtomics
        false,                                              // areMultipleSubDevicesInContext
        false                                               // overrideSurfaceStateBaseAddress
    };
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(0u, sbaCmd.getL1CachePolicyL1CacheControl());

    DebugManager.flags.ForceStatelessL1CachingPolicy.set(2u);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(2u, sbaCmd.getL1CachePolicyL1CacheControl());

    DebugManager.flags.ForceAllResourcesUncached.set(true);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(1u, sbaCmd.getL1CachePolicyL1CacheControl());
}
