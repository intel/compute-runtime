/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/preamble/preamble_fixture.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "reg_configs_common.h"
#include "test_traits_common.h"

using namespace NEO;

using ThreadArbitrationXeHPAndLater = PreambleFixture;
using Platforms = IsWithinGfxCore<IGFX_XE_HP_CORE, IGFX_XE_HPG_CORE>;
HWTEST2_F(ThreadArbitrationXeHPAndLater, whenGetDefaultThreadArbitrationPolicyIsCalledThenCorrectPolicyIsReturned, Platforms) {
    EXPECT_EQ(ThreadArbitrationPolicy::AgeBased, HwHelperHw<FamilyType>::get().getDefaultThreadArbitrationPolicy());
}

using ProgramPipelineXeHPAndLater = PreambleFixture;
HWCMDTEST_F(IGFX_XE_HP_CORE, ProgramPipelineXeHPAndLater, whenCleanStateInPreambleIsSetAndProgramPipelineSelectIsCalledThenExtraPipelineSelectAndTwoExtraPipeControlsAdded) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    typedef typename FamilyType::PIPE_CONTROL PIPE_CONTROL;
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.CleanStateInPreamble.set(true);

    LinearStream &cs = linearStream;
    PipelineSelectArgs pipelineArgs;
    auto hwInfo = pDevice->getHardwareInfo();
    PreambleHelper<FamilyType>::programPipelineSelect(&cs, pipelineArgs, hwInfo);

    parseCommands<FamilyType>(cs, 0);
    auto numPipeControl = getCommandsList<PIPE_CONTROL>().size();
    EXPECT_EQ(2u, numPipeControl);
    auto numPipelineSelect = getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(2u, numPipelineSelect);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ProgramPipelineXeHPAndLater, givenDebugVariableWhenProgramPipelineSelectIsCalledThenItHasProperFieldsSet) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.OverrideSystolicPipelineSelect.set(1);

    LinearStream &cs = linearStream;
    PipelineSelectArgs pipelineArgs;
    auto hwInfo = pDevice->getHardwareInfo();
    PreambleHelper<FamilyType>::programPipelineSelect(&cs, pipelineArgs, hwInfo);

    parseCommands<FamilyType>(linearStream);

    auto itorCmd = find<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorCmd, cmdList.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_TRUE(cmd->getSystolicModeEnable());
}

using PreemptionWatermarkXeHPAndLater = PreambleFixture;
HWCMDTEST_F(IGFX_XE_HP_CORE, PreemptionWatermarkXeHPAndLater, givenPreambleThenPreambleWorkAroundsIsNotProgrammed) {
    PreambleHelper<FamilyType>::programGenSpecificPreambleWorkArounds(&linearStream, *defaultHwInfo);

    parseCommands<FamilyType>(linearStream);

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), FfSliceCsChknReg2::address);
    ASSERT_EQ(nullptr, cmd);

    MockDevice mockDevice;
    mockDevice.setDebuggerActive(false);
    size_t expectedSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(mockDevice);
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(mockDevice));

    mockDevice.setDebuggerActive(true);
    expectedSize += PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(mockDevice.isDebuggerActive());
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(mockDevice));
}

struct KernelCommandsXeHPAndLater : public PreambleVfeState {
    void SetUp() override {
        PreambleVfeState::SetUp();
        pDevice->incRefInternal();
        pClDevice = new MockClDevice{pDevice};
        ASSERT_NE(nullptr, pClDevice);

        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
    }

    void TearDown() override {
        pClDevice->decRefInternal();
        PreambleVfeState::TearDown();
    }

    MockClDevice *pClDevice = nullptr;
    std::unique_ptr<MockProgram> program;
    KernelInfo kernelInfo;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, KernelCommandsXeHPAndLater, whenKernelSizeIsRequiredThenReturnZero) {
    MockKernel kernel(program.get(), kernelInfo, *pClDevice);

    size_t expectedSize = 0;
    size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredCS();
    EXPECT_EQ(expectedSize, actualSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, KernelCommandsXeHPAndLater, whenPipeControlForWaIsRequiredThenReturnFalse) {
    auto &hwInfo = pDevice->getHardwareInfo();
    EXPECT_EQ(UnitTestHelper<FamilyType>::isPipeControlWArequired(hwInfo), MemorySynchronizationCommands<FamilyType>::isPipeControlWArequired(hwInfo));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, KernelCommandsXeHPAndLater, whenMediaInterfaceDescriptorLoadIsRequiredThenDoNotProgramNonExistingCommand) {
    size_t expectedSize = 0;
    EXPECT_EQ(expectedSize, linearStream.getUsed());
    HardwareCommandsHelper<FamilyType>::sendMediaInterfaceDescriptorLoad(linearStream, 0, 0);
    EXPECT_EQ(expectedSize, linearStream.getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, KernelCommandsXeHPAndLater, whenMediaStateFlushIsRequiredThenDoNotProgramNonExistingCommand) {
    size_t expectedSize = 0;
    EXPECT_EQ(expectedSize, linearStream.getUsed());
    HardwareCommandsHelper<FamilyType>::sendMediaStateFlush(linearStream, 0);
    EXPECT_EQ(expectedSize, linearStream.getUsed());
}

using PreambleCfeStateXeHPAndLater = PreambleFixture;

HWCMDTEST_F(IGFX_XE_HP_CORE, PreambleCfeStateXeHPAndLater, givenScratchEnabledWhenPreambleCfeStateIsProgrammedThenCheckMaxThreadsAddressFieldsAreProgrammed) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    uint32_t expectedMaxThreads = HwHelper::getMaxThreadsForVfe(*defaultHwInfo);
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, expectedAddress, expectedMaxThreads, emptyProperties);

    parseCommands<FamilyType>(linearStream);

    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);

    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_EQ(expectedMaxThreads, cfeState->getMaximumNumberOfThreads());
    uint64_t address = cfeState->getScratchSpaceBuffer();
    EXPECT_EQ(expectedAddress, address);
}

HWTEST2_F(PreambleCfeStateXeHPAndLater, givenNotSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveNotSetValue, IsAtLeastXeHpCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto cfeState = reinterpret_cast<CFE_STATE *>(linearStream.getSpace(sizeof(CFE_STATE)));
    *cfeState = FamilyType::cmdInitCfeState;

    [[maybe_unused]] uint32_t numberOfWalkers = 0u;
    [[maybe_unused]] uint32_t fusedEuDispach = 0u;
    if constexpr (TestTraits<gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
        numberOfWalkers = cfeState->getNumberOfWalkers();
    }
    if constexpr (TestTraits<gfxCoreFamily>::fusedEuDispatchSupported) {
        fusedEuDispach = cfeState->getFusedEuDispatch();
    }
    uint32_t overDispatchControl = static_cast<uint32_t>(cfeState->getOverDispatchControl());

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    uint32_t expectedMaxThreads = HwHelper::getMaxThreadsForVfe(*defaultHwInfo);
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, expectedAddress, expectedMaxThreads, emptyProperties);
    uint32_t maximumNumberOfThreads = cfeState->getMaximumNumberOfThreads();

    if constexpr (TestTraits<gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
        EXPECT_EQ(numberOfWalkers, cfeState->getNumberOfWalkers());
    }
    if constexpr (TestTraits<gfxCoreFamily>::fusedEuDispatchSupported) {
        EXPECT_EQ(fusedEuDispach, cfeState->getFusedEuDispatch());
    }
    EXPECT_NE(expectedMaxThreads, maximumNumberOfThreads);
    EXPECT_EQ(overDispatchControl, static_cast<uint32_t>(cfeState->getOverDispatchControl()));
}

HWTEST2_F(PreambleCfeStateXeHPAndLater, givenSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveSetValue, IsAtLeastXeHpCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    uint32_t expectedValue1 = 1u;
    uint32_t expectedValue2 = 2u;

    DebugManagerStateRestore dbgRestore;

    DebugManager.flags.CFEFusedEUDispatch.set(expectedValue1);
    DebugManager.flags.CFEOverDispatchControl.set(expectedValue1);
    DebugManager.flags.CFESingleSliceDispatchCCSMode.set(expectedValue1);
    DebugManager.flags.CFELargeGRFThreadAdjustDisable.set(expectedValue1);
    DebugManager.flags.CFENumberOfWalkers.set(expectedValue2);
    DebugManager.flags.CFEMaximumNumberOfThreads.set(expectedValue2);

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo, 0u, expectedAddress, 16u, emptyProperties);

    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);

    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_EQ(expectedValue1, static_cast<uint32_t>(cfeState->getOverDispatchControl()));
    EXPECT_EQ(expectedValue1, cfeState->getLargeGRFThreadAdjustDisable());
    if constexpr (TestTraits<gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
        EXPECT_EQ(expectedValue2, cfeState->getNumberOfWalkers());
    }
    EXPECT_EQ(expectedValue2, cfeState->getMaximumNumberOfThreads());
}

using XeHpCommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, whenFlushingCommandStreamReceiverThenExpectStateBaseAddressEqualsIndirectObjectBaseAddress) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    if constexpr (is64bit) {
        EXPECT_EQ(commandStreamReceiver.getMemoryManager()->getInternalHeapBaseAddress(commandStreamReceiver.rootDeviceIndex, ioh.getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
                  stateBaseAddress->getGeneralStateBaseAddress());
    } else {
        EXPECT_EQ(0u, stateBaseAddress->getGeneralStateBaseAddress());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, whenFlushCalledThenStateBaseAddressHasAllCachesOn) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto gmmHelper = pDevice->getRootDeviceEnvironment().getGmmHelper();

    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    auto expectedMocsForStateless = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);
    auto expectedMocsForHeap = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER);

    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getSurfaceStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getDynamicStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getGeneralStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getInstructionMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getBindlessSurfaceStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getBindlessSamplerStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForStateless, stateBaseAddress->getStatelessDataPortAccessMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, whenFlushCalledThenStateBaseAddressHasAllCachesOffWhenDebugFlagIsPresent) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.DisableCachingForHeaps.set(1);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto gmmHelper = pDevice->getRootDeviceEnvironment().getGmmHelper();

    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    auto expectedMocsForHeap = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED);

    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getSurfaceStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getDynamicStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getGeneralStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getInstructionMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getBindlessSurfaceStateMemoryObjectControlState());
    EXPECT_EQ(expectedMocsForHeap, stateBaseAddress->getBindlessSamplerStateMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, givenL3ToL1DebugFlagWhenStatelessMocsIsProgrammedThenItHasL1CachingOn) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(1u);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    auto expectedMocs = pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST);

    EXPECT_EQ(expectedMocs, stateBaseAddress->getStatelessDataPortAccessMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, givenForceL1CachingDebugFlagDisabledWhenStatelessMocsIsProgrammedThenItHasL3CachingOn) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceL1Caching.set(0u);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    auto expectedMocs = pDevice->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);

    EXPECT_EQ(expectedMocs, stateBaseAddress->getStatelessDataPortAccessMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, whenFlushingCommandStreamReceiverThenExpectBindlessBaseAddressEqualSurfaceStateBaseAddress) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    auto surfaceStateBaseAddress = ssh.getHeapGpuBase();
    EXPECT_EQ(surfaceStateBaseAddress, stateBaseAddress->getBindlessSurfaceStateBaseAddress());
    EXPECT_EQ(surfaceStateBaseAddress, stateBaseAddress->getSurfaceStateBaseAddress());
    uint32_t bindlessSurfaceSize = static_cast<uint32_t>(ssh.getMaxAvailableSpace() / sizeof(RENDER_SURFACE_STATE)) - 1;
    EXPECT_EQ(bindlessSurfaceSize, stateBaseAddress->getBindlessSurfaceStateSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, whenFlushingCommandStreamReceiverThenSetBindlessSamplerStateBaseAddressModifyEnable) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    EXPECT_TRUE(stateBaseAddress->getBindlessSamplerStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, stateBaseAddress->getBindlessSamplerStateBaseAddress());
    EXPECT_EQ(0u, stateBaseAddress->getBindlessSamplerStateBufferSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, givenMultEngineQueueFalseWhenFlushingCommandStreamReceiverThenSetPartialWriteFieldsTrue) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    EXPECT_TRUE(stateBaseAddress->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
    EXPECT_TRUE(stateBaseAddress->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
}

struct MultiGpuGlobalAtomicsTest : public XeHpCommandStreamReceiverFlushTaskTests,
                                   public ::testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
};

HWCMDTEST_P(IGFX_XE_HP_CORE, MultiGpuGlobalAtomicsTest, givenFlushingCommandStreamReceiverThenDisableSupportForMultiGpuAtomicsForStatelessAccessesIsSetCorrectly) {
    bool isMultiOsContextCapable, useGlobalAtomics, areMultipleSubDevicesInContext, enableMultiGpuAtomicsOptimization;
    std::tie(isMultiOsContextCapable, useGlobalAtomics, areMultipleSubDevicesInContext, enableMultiGpuAtomicsOptimization) = GetParam();

    DebugManagerStateRestore stateRestore;
    DebugManager.flags.EnableMultiGpuAtomicsOptimization.set(enableMultiGpuAtomicsOptimization);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.multiOsContextCapable = isMultiOsContextCapable;
    flushTaskFlags.useGlobalAtomics = useGlobalAtomics;
    flushTaskFlags.areMultipleSubDevicesInContext = areMultipleSubDevicesInContext;

    flushTask(commandStreamReceiver, false, 0, false, false);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);

    auto enabled = isMultiOsContextCapable;
    if (enableMultiGpuAtomicsOptimization) {
        enabled = useGlobalAtomics && (enabled || areMultipleSubDevicesInContext);
    }

    EXPECT_EQ(!enabled, stateBaseAddress->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

INSTANTIATE_TEST_CASE_P(MultiGpuGlobalAtomics,
                        MultiGpuGlobalAtomicsTest,
                        ::testing::Combine(
                            ::testing::Bool(),
                            ::testing::Bool(),
                            ::testing::Bool(),
                            ::testing::Bool()));

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, givenDebugKeysThatOverrideMultiGpuSettingWhenStateBaseAddressIsProgrammedThenValuesMatch) {
    DebugManagerStateRestore restorer;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DebugManager.flags.ForceMultiGpuAtomics.set(0);
    DebugManager.flags.ForceMultiGpuPartialWrites.set(0);
    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    EXPECT_EQ(0u, stateBaseAddress->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
    EXPECT_EQ(0u, stateBaseAddress->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, givenMultEngineQueueTrueWhenFlushingCommandStreamReceiverThenSetPartialWriteFieldsFalse) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.multiOsContextCapable = true;

    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    EXPECT_TRUE(stateBaseAddress->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
    EXPECT_FALSE(stateBaseAddress->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
}

using StateBaseAddressXeHPAndLaterTests = XeHpCommandStreamReceiverFlushTaskTests;

struct CompressionParamsSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::surfaceStateCompressionParamsSupported;
        }
        return false;
    }
};

HWTEST2_F(StateBaseAddressXeHPAndLaterTests, givenMemoryCompressionEnabledWhenAppendingSbaThenEnableStatelessCompressionForAllStatelessAccesses, CompressionParamsSupportedMatcher) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), 1, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    IndirectHeap indirectHeap(allocation, 1);

    for (auto memoryCompressionState : {MemoryCompressionState::NotApplicable, MemoryCompressionState::Disabled, MemoryCompressionState::Enabled}) {
        auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(&sbaCmd, &indirectHeap, true, 0,
                                                                             pDevice->getRootDeviceEnvironment().getGmmHelper(), false, memoryCompressionState, true, false, 1u);
        if (memoryCompressionState == MemoryCompressionState::Enabled) {
            EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_ENABLED, sbaCmd.getEnableMemoryCompressionForAllStatelessAccesses());
        } else {
            EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED, sbaCmd.getEnableMemoryCompressionForAllStatelessAccesses());
        }
    }

    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, StateBaseAddressXeHPAndLaterTests, givenNonZeroInternalHeapBaseAddressWhenSettingIsDisabledThenExpectCommandValueZero) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), 1, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    IndirectHeap indirectHeap(allocation, 1);
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    uint64_t ihba = 0x80010000ull;
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(&sbaCmd, &indirectHeap, false, ihba,
                                                                         pDevice->getRootDeviceEnvironment().getGmmHelper(), false,
                                                                         MemoryCompressionState::NotApplicable, true, false, 1u);
    EXPECT_EQ(0ull, sbaCmd.getGeneralStateBaseAddress());
    memoryManager->freeGraphicsMemory(allocation);
}

using RenderSurfaceStateXeHPAndLaterTests = XeHpCommandStreamReceiverFlushTaskTests;

HWCMDTEST_F(IGFX_XE_HP_CORE, RenderSurfaceStateXeHPAndLaterTests, givenSpecificProductFamilyWhenAppendingRssThenProgramGpuCoherency) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    MockContext context(pClDevice);
    auto multiGraphicsAllocation = MultiGraphicsAllocation(pClDevice->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(allocation);

    std::unique_ptr<BufferHw<FamilyType>> buffer(static_cast<BufferHw<FamilyType> *>(
        BufferHw<FamilyType>::create(&context, {}, 0, 0, allocationSize, nullptr, nullptr, multiGraphicsAllocation, false, false, false)));

    NEO::EncodeSurfaceStateArgs args;
    args.outMemory = &rssCmd;
    args.graphicsAddress = allocation->getGpuAddress();
    args.size = allocation->getUnderlyingBufferSize();
    args.mocs = buffer->getMocsValue(false, false, pClDevice->getRootDeviceIndex());
    args.numAvailableDevices = pClDevice->getNumGenericSubDevices();
    args.allocation = allocation;
    args.gmmHelper = pClDevice->getGmmHelper();
    args.areMultipleSubDevicesInContext = true;

    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(FamilyType::RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, rssCmd.getCoherencyType());
}

using PipelineSelectTest = ::testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, PipelineSelectTest, whenCallingIsSpecialPipelineSelectModeChangedThenReturnCorrectValue) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    bool oldPipelineSelectSpecialMode = true;
    bool newPipelineSelectSpecialMode = false;

    auto result = PreambleHelper<FamilyType>::isSpecialPipelineSelectModeChanged(oldPipelineSelectSpecialMode, newPipelineSelectSpecialMode, *defaultHwInfo);
    EXPECT_TRUE(result);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PipelineSelectTest, WhenProgramPipelineSelectThenProperMaskIsSet) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    PIPELINE_SELECT cmd = FamilyType::cmdInitPipelineSelect;
    LinearStream pipelineSelectStream(&cmd, sizeof(cmd));
    PreambleHelper<FamilyType>::programPipelineSelect(&pipelineSelectStream, {}, *defaultHwInfo);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits;
    if constexpr (FamilyType::isUsingMediaSamplerDopClockGate) {
        expectedMask |= pipelineSelectMediaSamplerDopClockGateMaskBits;
    }

    if (PreambleHelper<FamilyType>::isSystolicModeConfigurable(*defaultHwInfo)) {
        expectedMask |= pipelineSelectSystolicModeEnableMaskBits;
    }

    EXPECT_EQ(expectedMask, cmd.getMaskBits());
}