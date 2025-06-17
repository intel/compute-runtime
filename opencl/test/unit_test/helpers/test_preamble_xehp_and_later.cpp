/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/fixtures/preamble_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "encode_surface_state_args.h"
#include "test_traits_common.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

using ThreadArbitrationXeHPAndLater = PreambleFixture;
HWTEST2_F(ThreadArbitrationXeHPAndLater, whenGetDefaultThreadArbitrationPolicyIsCalledThenCorrectPolicyIsReturned, IsXeHpgCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    EXPECT_EQ(ThreadArbitrationPolicy::AgeBased, gfxCoreHelper.getDefaultThreadArbitrationPolicy());
}

using ProgramPipelineXeHPAndLater = PreambleFixture;
HWTEST2_F(ProgramPipelineXeHPAndLater, givenDebugVariableWhenProgramPipelineSelectIsCalledThenItHasProperFieldsSet, IsXeCore) {
    typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
    DebugManagerStateRestore stateRestore;
    debugManager.flags.OverrideSystolicPipelineSelect.set(1);

    LinearStream &cs = linearStream;
    PipelineSelectArgs pipelineArgs;
    PreambleHelper<FamilyType>::programPipelineSelect(&cs, pipelineArgs, pDevice->getRootDeviceEnvironment());

    parseCommands<FamilyType>(linearStream);

    auto itorCmd = find<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorCmd, cmdList.end());

    auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorCmd);
    EXPECT_TRUE(cmd->getSystolicModeEnable());
}

using PreemptionWatermarkXeHPAndLater = PreambleFixture;
HWTEST2_F(PreemptionWatermarkXeHPAndLater, givenPreambleThenPreambleWorkAroundsIsNotProgrammed, IsAtLeastXeCore) {
    PreambleHelper<FamilyType>::programGenSpecificPreambleWorkArounds(&linearStream, *defaultHwInfo);

    parseCommands<FamilyType>(linearStream);

    auto cmd = findMmioCmd<FamilyType>(cmdList.begin(), cmdList.end(), FfSliceCsChknReg2::address);
    ASSERT_EQ(nullptr, cmd);

    MockDevice mockDevice;
    size_t expectedSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(mockDevice);
    EXPECT_EQ(expectedSize, PreambleHelper<FamilyType>::getAdditionalCommandsSize(mockDevice));

    mockDevice.executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(&mockDevice);
    expectedSize = PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(mockDevice.getDebugger() != nullptr);
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
        program.reset();
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
    EXPECT_EQ(UnitTestHelper<FamilyType>::isPipeControlWArequired(hwInfo), MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(pDevice->getRootDeviceEnvironment()));
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
    if constexpr (FamilyType::isHeaplessRequired()) {
        GTEST_SKIP();
    } else {
        using CFE_STATE = typename FamilyType::CFE_STATE;

        uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
        uint32_t expectedMaxThreads = GfxCoreHelper::getMaxThreadsForVfe(*defaultHwInfo);
        auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::renderCompute);
        StreamProperties emptyProperties{};
        PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, expectedAddress, expectedMaxThreads, emptyProperties);

        parseCommands<FamilyType>(linearStream);

        auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), cfeStateIt);

        auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

        EXPECT_EQ(expectedMaxThreads, cfeState->getMaximumNumberOfThreads());
        uint64_t address = cfeState->getScratchSpaceBuffer();
        EXPECT_EQ(expectedAddress, address);
    }
}

HWTEST2_F(PreambleCfeStateXeHPAndLater, givenNotSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveNotSetValue, IsHeapfulSupportedAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    auto cfeState = reinterpret_cast<CFE_STATE *>(linearStream.getSpace(sizeof(CFE_STATE)));
    *cfeState = FamilyType::cmdInitCfeState;

    [[maybe_unused]] uint32_t numberOfWalkers = 0u;
    [[maybe_unused]] uint32_t fusedEuDispach = 0u;
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
        numberOfWalkers = cfeState->getNumberOfWalkers();
    }
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::fusedEuDispatchSupported) {
        fusedEuDispach = cfeState->getFusedEuDispatch();
    }
    uint32_t overDispatchControl = static_cast<uint32_t>(cfeState->getOverDispatchControl());

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    uint32_t expectedMaxThreads = GfxCoreHelper::getMaxThreadsForVfe(*defaultHwInfo);
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::renderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, expectedAddress, expectedMaxThreads, emptyProperties);
    uint32_t maximumNumberOfThreads = cfeState->getMaximumNumberOfThreads();

    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
        EXPECT_EQ(numberOfWalkers, cfeState->getNumberOfWalkers());
    }
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::fusedEuDispatchSupported) {
        EXPECT_EQ(fusedEuDispach, cfeState->getFusedEuDispatch());
    }
    EXPECT_NE(expectedMaxThreads, maximumNumberOfThreads);
    EXPECT_EQ(overDispatchControl, static_cast<uint32_t>(cfeState->getOverDispatchControl()));
}

HWTEST2_F(PreambleCfeStateXeHPAndLater, givenSetDebugFlagWhenPreambleCfeStateIsProgrammedThenCFEStateParamsHaveSetValue, IsWithinXeCoreAndXe2HpgCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    uint32_t expectedValue1 = 1u;
    uint32_t expectedValue2 = 2u;

    DebugManagerStateRestore dbgRestore;

    debugManager.flags.CFEFusedEUDispatch.set(expectedValue1);
    debugManager.flags.OverDispatchControl.set(expectedValue1);
    debugManager.flags.CFESingleSliceDispatchCCSMode.set(expectedValue1);
    debugManager.flags.CFELargeGRFThreadAdjustDisable.set(expectedValue1);
    debugManager.flags.CFENumberOfWalkers.set(expectedValue2);
    debugManager.flags.MaximumNumberOfThreads.set(expectedValue2);

    uint64_t expectedAddress = 1 << CFE_STATE::SCRATCHSPACEBUFFER_BIT_SHIFT;
    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&linearStream, *defaultHwInfo, EngineGroupType::renderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, pDevice->getRootDeviceEnvironment(), 0u, expectedAddress, 16u, emptyProperties);

    parseCommands<FamilyType>(linearStream);
    auto cfeStateIt = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), cfeStateIt);

    auto cfeState = reinterpret_cast<CFE_STATE *>(*cfeStateIt);

    EXPECT_EQ(expectedValue1, static_cast<uint32_t>(cfeState->getOverDispatchControl()));
    EXPECT_EQ(expectedValue1, cfeState->getLargeGRFThreadAdjustDisable());
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
        EXPECT_EQ(expectedValue2, cfeState->getNumberOfWalkers());
    }
    EXPECT_EQ(expectedValue2, cfeState->getMaximumNumberOfThreads());
}

using XeHpCommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, whenFlushingCommandStreamReceiverThenExpectStateBaseAddressEqualsIndirectObjectBaseAddress) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
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
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    auto expectedMocsForStateless = gmmHelper->getL1EnabledMOCS();
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
    debugManager.flags.DisableCachingForHeaps.set(1);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
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
    debugManager.flags.ForceL1Caching.set(1u);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    auto expectedMocs = pDevice->getGmmHelper()->getL1EnabledMOCS();

    EXPECT_EQ(expectedMocs, stateBaseAddress->getStatelessDataPortAccessMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, givenForceL1CachingDebugFlagDisabledWhenStatelessMocsIsProgrammedThenItHasL3CachingOn) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(0u);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    auto expectedMocs = pDevice->getGmmHelper()->getL3EnabledMOCS();

    EXPECT_EQ(expectedMocs, stateBaseAddress->getStatelessDataPortAccessMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, whenFlushingCommandStreamReceiverThenExpectBindlessBaseAddressEqualSurfaceStateBaseAddress) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }
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
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessModeEnabled) {
        GTEST_SKIP();
    }

    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);
    EXPECT_TRUE(stateBaseAddress->getBindlessSamplerStateBaseAddressModifyEnable());
    EXPECT_EQ(dsh.getHeapGpuBase(), stateBaseAddress->getBindlessSamplerStateBaseAddress());
    EXPECT_EQ(dsh.getHeapSizeInPages(), stateBaseAddress->getBindlessSamplerStateBufferSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHpCommandStreamReceiverFlushTaskTests, givenDebugKeysThatOverrideMultiGpuSettingWhenStateBaseAddressIsProgrammedThenValuesMatch) {
    DebugManagerStateRestore restorer;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    flushTask(commandStreamReceiver);
    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
}

using StateBaseAddressXeHPAndLaterTests = XeHpCommandStreamReceiverFlushTaskTests;

template <typename FamilyType, typename CommandStreamReceiverType>
void flushTaskAndcheckForSBA(StateBaseAddressXeHPAndLaterTests *sbaTest, CommandStreamReceiverType &csr, bool shouldBePresent) {
    size_t offset = csr.commandStream.getUsed();

    sbaTest->flushTask(csr);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(csr.commandStream, offset);
    hwParserCsr.findHardwareCommands<FamilyType>();
    if (shouldBePresent) {
        EXPECT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);
    } else {
        EXPECT_EQ(nullptr, hwParserCsr.cmdStateBaseAddress);
    }
}

using RenderSurfaceStateXeHPAndLaterTests = XeHpCommandStreamReceiverFlushTaskTests;

HWTEST2_F(RenderSurfaceStateXeHPAndLaterTests, givenSpecificProductFamilyWhenAppendingRssThenProgramGpuCoherency, IsXeCore) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::buffer, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    MockContext context(pClDevice);
    auto multiGraphicsAllocation = MultiGraphicsAllocation(pClDevice->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(allocation);

    std::unique_ptr<BufferHw<FamilyType>> buffer(static_cast<BufferHw<FamilyType> *>(
        BufferHw<FamilyType>::create(&context, {}, 0, 0, allocationSize, nullptr, nullptr, std::move(multiGraphicsAllocation), false, false, false)));

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

HWTEST2_F(PipelineSelectTest, WhenProgramPipelineSelectThenProperMaskIsSet, IsXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    std::vector<uint8_t> linearStreamBackingMemory;
    size_t sizeNeededForCommandSream = 0;
    if (MemorySynchronizationCommands<FamilyType>::isBarrierPriorToPipelineSelectWaRequired(rootDeviceEnvironment)) {
        sizeNeededForCommandSream += MemorySynchronizationCommands<FamilyType>::getSizeForSingleBarrier();
    }

    sizeNeededForCommandSream += sizeof(PIPELINE_SELECT);
    linearStreamBackingMemory.resize(linearStreamBackingMemory.size() + sizeNeededForCommandSream);

    PIPELINE_SELECT *cmd = reinterpret_cast<PIPELINE_SELECT *>(linearStreamBackingMemory.data() + linearStreamBackingMemory.size() - sizeof(PIPELINE_SELECT));
    *cmd = FamilyType::cmdInitPipelineSelect;

    LinearStream pipelineSelectStream(linearStreamBackingMemory.data(), linearStreamBackingMemory.size());

    PipelineSelectArgs pipelineArgs = {};
    pipelineArgs.systolicPipelineSelectSupport = PreambleHelper<FamilyType>::isSystolicModeConfigurable(rootDeviceEnvironment);

    PreambleHelper<FamilyType>::programPipelineSelect(&pipelineSelectStream, pipelineArgs, rootDeviceEnvironment);

    auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits;
    if constexpr (FamilyType::isUsingMediaSamplerDopClockGate) {
        expectedMask |= pipelineSelectMediaSamplerDopClockGateMaskBits;
    }

    if (pipelineArgs.systolicPipelineSelectSupport) {
        expectedMask |= pipelineSelectSystolicModeEnableMaskBits;
    }

    EXPECT_EQ(expectedMask, cmd->getMaskBits());

    HardwareParse hwParser;
    hwParser.parsePipeControl = true;
    hwParser.parseCommands<FamilyType>(pipelineSelectStream, 0);
    hwParser.findHardwareCommands<FamilyType>();

    if (MemorySynchronizationCommands<FamilyType>::isBarrierPriorToPipelineSelectWaRequired(rootDeviceEnvironment)) {
        bool pcWithRenderFlushFound = false;

        auto itorPipeControl = hwParser.pipeControlList.begin();
        if (itorPipeControl != hwParser.pipeControlList.end()) {
            auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*itorPipeControl);
            if (pipeControl->getRenderTargetCacheFlushEnable()) {
                pcWithRenderFlushFound = true;
            }
        }

        EXPECT_TRUE(pcWithRenderFlushFound);
    }
}
