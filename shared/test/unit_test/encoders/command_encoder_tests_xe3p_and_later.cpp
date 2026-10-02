/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/flush_caches_bitmask.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "implicit_args.h"

using namespace NEO;

using CommandEncodeStatesTestXe3pAndLater = Test<CommandEncodeStatesFixture>;

struct IsAtLeastXe3pCoreWithLscSamplerBackingThreshold {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (IsAtLeastXe3pCore::isMatched<productFamily>()) {
            using FamilyType = typename NEO::GfxFamilyMapper<NEO::ToGfxCoreFamily<productFamily>::get()>::GfxFamily;
            return requires { typename FamilyType::STATE_COMPUTE_MODE::LSC_SAMPLER_BACKING_THRESHOLD; };
        }
        return false;
    }
};

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenDebugFlagSetWhenProgrammingSemaphoreSectionThenSetSwitchMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using QUEUE_SWITCH_MODE = typename MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE;

    DebugManagerStateRestore restore;

    {
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        bool ret = directSubmission.initialize(false);
        EXPECT_TRUE(ret);

        auto &cmdStream = directSubmission.ringCommandStream;
        auto offset = cmdStream.getUsed();
        GenCmdList cmdList;

        directSubmission.dispatchSemaphoreSection(1u);

        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), offset), (cmdStream.getUsed() - offset)));

        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphore);

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());
    }
    {
        debugManager.flags.DirectSubmissionSwitchSemaphoreMode.set(1);
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        bool ret = directSubmission.initialize(false);
        EXPECT_TRUE(ret);

        auto &cmdStream = directSubmission.ringCommandStream;
        auto offset = cmdStream.getUsed();
        GenCmdList cmdList;

        directSubmission.dispatchSemaphoreSection(1u);

        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), offset), (cmdStream.getUsed() - offset)));

        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphore);

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
    }
    {
        debugManager.flags.DirectSubmissionSwitchSemaphoreMode.set(0);
        MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
        bool ret = directSubmission.initialize(false);
        EXPECT_TRUE(ret);

        auto &cmdStream = directSubmission.ringCommandStream;
        auto offset = cmdStream.getUsed();
        GenCmdList cmdList;

        directSubmission.dispatchSemaphoreSection(1u);

        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), offset), (cmdStream.getUsed() - offset)));

        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphore);

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenHighestPriorityLevelSetWhenProgrammingSemaphoreSectionThenSetSwitchMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using QUEUE_SWITCH_MODE = typename MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE;

    struct MockOsContext : public OsContext {
        using OsContext::priorityLevel;
    };
    auto highestPriority = pDevice->getGfxCoreHelper().getHwQueuePriority(pDevice->getGfxCoreHelper().getHighestQueuePriorityLevel());
    reinterpret_cast<MockOsContext *>(pDevice->getDefaultEngine().osContext)->priorityLevel = highestPriority;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    auto &cmdStream = directSubmission.ringCommandStream;
    auto offset = cmdStream.getUsed();
    GenCmdList cmdList;

    directSubmission.dispatchSemaphoreSection(1u);

    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), offset), (cmdStream.getUsed() - offset)));

    auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphore);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenHighPriorityContextSetWhenProgrammingSemaphoreSectionThenSetSwitchMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using QUEUE_SWITCH_MODE = typename MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE;

    pDevice->getDefaultEngine().osContext->overrideEngineUsage(EngineUsage::highPriority);

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);
    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    auto &cmdStream = directSubmission.ringCommandStream;
    auto offset = cmdStream.getUsed();
    GenCmdList cmdList;

    directSubmission.dispatchSemaphoreSection(1u);

    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), offset), (cmdStream.getUsed() - offset)));

    auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), semaphore);

    auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
    ASSERT_NE(nullptr, semaphoreCmd);

    EXPECT_EQ(QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenProgramBatchBufferStartCommandWhenItIsCalledThenCommandIsProgrammedCorrectly, IsAtLeastXe3pCore) {
    constexpr auto expectedUsedSize = sizeof(WalkerPartition::BATCH_BUFFER_START<FamilyType>);
    uint64_t gpuAddress = 0xFFFFFFDFEEDBAC10llu;

    uint8_t cmdBuffer[2 * expectedUsedSize] = {};
    void *cmdBufferAddress = cmdBuffer;
    uint32_t totalBytesProgrammed = 0;

    void *batchBufferStartAddress = cmdBufferAddress;
    WalkerPartition::programMiBatchBufferStart<FamilyType>(cmdBufferAddress, totalBytesProgrammed, gpuAddress, true, false);
    auto batchBufferStart = genCmdCast<WalkerPartition::BATCH_BUFFER_START<FamilyType> *>(batchBufferStartAddress);
    ASSERT_NE(nullptr, batchBufferStart);
    EXPECT_EQ(expectedUsedSize, totalBytesProgrammed);

    EXPECT_EQ(gpuAddress, batchBufferStart->getBatchBufferStartAddress());

    EXPECT_TRUE(batchBufferStart->getPredicationEnable());
    EXPECT_FALSE(batchBufferStart->getEnableCommandCache());
    EXPECT_EQ(WalkerPartition::BATCH_BUFFER_START<FamilyType>::SECOND_LEVEL_BATCH_BUFFER::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH, batchBufferStart->getSecondLevelBatchBuffer());
    EXPECT_EQ(WalkerPartition::BATCH_BUFFER_START<FamilyType>::ADDRESS_SPACE_INDICATOR::ADDRESS_SPACE_INDICATOR_PPGTT, batchBufferStart->getAddressSpaceIndicator());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenEncodeDispatchKernelWhenRequestingCommandViewThenDoNotProgramIndirectPointer, IsAtLeastXe3pCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0u;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = 8u;

    auto payloadHeap = cmdContainer->getIndirectHeap(HeapType::indirectObject);
    auto payloadHeapUsed = payloadHeap->getUsed();

    auto cmdBuffer = cmdContainer->getCommandStream();
    auto cmdBufferUsed = cmdBuffer->getUsed();

    uint8_t payloadView[256] = {};
    dispatchInterface->getCrossThreadDataSizeResult = 64;

    auto walkerPtr = std::make_unique<DefaultWalkerType>();
    DefaultWalkerType *cpuWalkerPointer = walkerPtr.get();

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.makeCommandView = true;
    dispatchArgs.cpuPayloadBuffer = payloadView;
    dispatchArgs.cpuWalkerBuffer = cpuWalkerPointer;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    EXPECT_EQ(payloadHeapUsed, payloadHeap->getUsed());
    EXPECT_EQ(cmdBufferUsed, cmdBuffer->getUsed());

    uint8_t *inlineDataIndirectPointerOffset = reinterpret_cast<uint8_t *>(cpuWalkerPointer->getInlineDataPointer()) +
                                               dispatchInterface->getKernelDescriptor().payloadMappings.implicitArgs.indirectDataPointerAddress.offset;
    uint64_t *indirectPointer = reinterpret_cast<uint64_t *>(inlineDataIndirectPointerOffset);
    EXPECT_EQ(0u, *indirectPointer);
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenComputeWalker2WhenEncodingWalkerThenIohIsCorrectlyAligned, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    auto heap = cmdContainer->getIndirectHeap(HeapType::indirectObject);
    auto usedBefore = heap->getUsed();

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);

    auto usedAfter = heap->getUsed();
    auto iohDiff = usedAfter - usedBefore;
    auto iohDiffAligned = alignUp(iohDiff, NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment(false, pDevice->getHardwareInfo()));

    EXPECT_EQ(iohDiffAligned, iohDiff);
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenDebugFlagSetWhenLSCSamplerBackingThresholdThenCorrectValueIsSet, IsAtLeastXe3pCoreWithLscSamplerBackingThreshold) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using LSC_SAMPLER_BACKING_THRESHOLD = typename STATE_COMPUTE_MODE::LSC_SAMPLER_BACKING_THRESHOLD;

    DebugManagerStateRestore restore;

    uint8_t buffer[2 * sizeof(STATE_COMPUTE_MODE)] = {};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    {
        // default
        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
        EXPECT_EQ(LSC_SAMPLER_BACKING_THRESHOLD::LSC_SAMPLER_BACKING_THRESHOLD_LEVEL_0, stateComputeModeCmd.getLscSamplerBackingThreshold());
    }

    {
        // possible levels
        for (auto thresholdLevel : {0, 1, 2, 3}) {
            debugManager.flags.LSCSamplerBackingThreshold.set(thresholdLevel);

            LinearStream linearStream(buffer, sizeof(buffer));

            StreamProperties streamProperties{};
            streamProperties.initSupport(rootDeviceEnvironment);
            streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
            EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

            auto &stateComputeModeCmd = *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
            EXPECT_EQ(static_cast<LSC_SAMPLER_BACKING_THRESHOLD>(thresholdLevel), stateComputeModeCmd.getLscSamplerBackingThreshold());
        }
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenDebugFlagWhenProgrammingStateComputeModeThenTdlRowArbitrationPolicyIsOverridden, IsAtLeastXe3pCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using TDL_ROW_ARBITRATION_POLICY = typename STATE_COMPUTE_MODE::TDL_ROW_ARBITRATION_POLICY;

    constexpr uint32_t tdlRowArbitrationPolicyMask = FamilyType::stateComputeModeTdlRowArbitrationPolicyMask;

    DebugManagerStateRestore restore;

    alignas(STATE_COMPUTE_MODE) uint8_t buffer[sizeof(STATE_COMPUTE_MODE)]{};
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    auto programStateComputeMode = [&]() -> STATE_COMPUTE_MODE & {
        LinearStream linearStream(buffer, sizeof(buffer));

        StreamProperties streamProperties{};
        streamProperties.initSupport(rootDeviceEnvironment);
        streamProperties.stateComputeMode.setPropertiesAll(false, 0, 0, PreemptionMode::Disabled, false);
        EncodeComputeMode<FamilyType>::programComputeModeCommand(linearStream, streamProperties.stateComputeMode, rootDeviceEnvironment);

        return *reinterpret_cast<STATE_COMPUTE_MODE *>(linearStream.getCpuBase());
    };

    {
        // default - neither the field nor its mask bit are touched
        auto &stateComputeModeCmd = programStateComputeMode();
        EXPECT_EQ(TDL_ROW_ARBITRATION_POLICY::TDL_ROW_ARBITRATION_POLICY_LEGACY_ROUND_ROBIN, stateComputeModeCmd.getTdlRowArbitrationPolicy());
        EXPECT_EQ(0u, stateComputeModeCmd.getMask2() & tdlRowArbitrationPolicyMask);
    }

    for (auto policy : {TDL_ROW_ARBITRATION_POLICY::TDL_ROW_ARBITRATION_POLICY_LEGACY_ROUND_ROBIN,
                        TDL_ROW_ARBITRATION_POLICY::TDL_ROW_ARBITRATION_POLICY_ORDERED_ROUND_ROBIN}) {
        debugManager.flags.ScmTdlRowArbitrationPolicyOverride.set(static_cast<int32_t>(policy));

        auto &stateComputeModeCmd = programStateComputeMode();
        EXPECT_EQ(policy, stateComputeModeCmd.getTdlRowArbitrationPolicy());
        EXPECT_EQ(tdlRowArbitrationPolicyMask, stateComputeModeCmd.getMask2() & tdlRowArbitrationPolicyMask);
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, whenAdjustSamplerStateBorderColorIsCalledThenBorderColorInSamplerStateIsCorrect, IsAtLeastXe3pCore) {

    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    using SAMPLER_BORDER_COLOR_STATE = typename FamilyType::SAMPLER_BORDER_COLOR_STATE;

    {
        SAMPLER_STATE samplerState{};
        SAMPLER_BORDER_COLOR_STATE borderColorState{};
        borderColorState.setBorderColorRed(0.0f);
        borderColorState.setBorderColorGreen(0.0f);
        borderColorState.setBorderColorBlue(0.0f);
        borderColorState.setBorderColorAlpha(1.0f);

        EncodeStates<FamilyType>::adjustSamplerStateBorderColor(samplerState, borderColorState);

        EXPECT_EQ(0.0f, samplerState.getBorderColorRed());
        EXPECT_EQ(0.0f, samplerState.getBorderColorGreen());
        EXPECT_EQ(0.0f, samplerState.getBorderColorBlue());
        EXPECT_EQ(1.0f, samplerState.getBorderColorAlpha());
    }

    {
        SAMPLER_STATE samplerState{};
        SAMPLER_BORDER_COLOR_STATE borderColorState{};
        borderColorState.setBorderColorRed(0.0f);
        borderColorState.setBorderColorGreen(0.0f);
        borderColorState.setBorderColorBlue(0.0f);
        borderColorState.setBorderColorAlpha(0.0f);

        EncodeStates<FamilyType>::adjustSamplerStateBorderColor(samplerState, borderColorState);

        EXPECT_EQ(0.0f, samplerState.getBorderColorRed());
        EXPECT_EQ(0.0f, samplerState.getBorderColorGreen());
        EXPECT_EQ(0.0f, samplerState.getBorderColorBlue());
        EXPECT_EQ(0.0f, samplerState.getBorderColorAlpha());
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, GivenComputeWalker2AndArgsWithL3FlushRequiredWhencallingEncodeL3FlushThenCorrectValuesAreSet, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    bool requiresUncachedMocs = false;
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WalkerType *>(*itor);

    for (bool flushL3ForExternalAllocation : {true, false}) {
        for (bool flushL3ForHostUsm : {true, false}) {
            auto &postSyncArgs = dispatchArgs.postSyncArgs;
            postSyncArgs.device = pDevice;
            postSyncArgs.dcFlushEnable = true;
            postSyncArgs.isFlushL3ForExternalAllocationRequired = flushL3ForExternalAllocation;
            postSyncArgs.isFlushL3ForHostUsmRequired = flushL3ForHostUsm;

            EncodePostSync<FamilyType>::template encodeL3Flush<WalkerType>(*walkerCmd, postSyncArgs);

            EXPECT_EQ(flushL3ForExternalAllocation, walkerCmd->getPostSync().getL2Flush());
            EXPECT_EQ(flushL3ForHostUsm, walkerCmd->getPostSync().getL2TransientFlush());
        }
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenL2FlushRequiredWhenCallingSetupPostSyncForInOrderExecThenPostSyncDataIsSetCorrectly, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    WalkerType walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, pDevice->getMemoryManager());
    auto inOrderExecInfo = InOrderExecInfo::create(deviceTagAllocator.getTag(), nullptr, *pDevice, 1);

    dispatchArgs.postSyncArgs.inOrderExecInfo = inOrderExecInfo.get();
    auto &postSyncArgs = dispatchArgs.postSyncArgs;

    for (bool dcFlushEnable : {true, false}) {
        for (bool flushL3ForExternalAllocation : {true, false}) {
            for (bool flushL3ForHostUsm : {true, false}) {

                postSyncArgs.dcFlushEnable = dcFlushEnable;
                postSyncArgs.isFlushL3ForExternalAllocationRequired = flushL3ForExternalAllocation;
                postSyncArgs.isFlushL3ForHostUsmRequired = flushL3ForHostUsm;

                EncodePostSync<FamilyType>::template setupPostSyncForInOrderExec<WalkerType>(walkerCmd, postSyncArgs);

                auto &postSyncData = walkerCmd.getPostSync();
                EXPECT_EQ(flushL3ForExternalAllocation && dcFlushEnable, postSyncData.getL2Flush());
                EXPECT_EQ(flushL3ForHostUsm && dcFlushEnable, postSyncData.getL2TransientFlush());
            }
        }
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, GivenComputeWalker2AndDefaultArgsWhencallingSetupPostSyncForInOrderExecThenCorrectValuesAreSet, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForcePostSyncL1Flush.set(0);
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    WalkerType walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, pDevice->getMemoryManager());

    auto inOrderExecInfo = InOrderExecInfo::create(deviceTagAllocator.getTag(), nullptr, *pDevice, 1);

    dispatchArgs.postSyncArgs.inOrderExecInfo = inOrderExecInfo.get();
    auto &postSyncArgs = dispatchArgs.postSyncArgs;
    EncodePostSync<FamilyType>::template setupPostSyncForInOrderExec<WalkerType>(walkerCmd, postSyncArgs);

    auto &postSyncData = walkerCmd.getPostSync();
    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::OPERATION_ATOMIC_OPN, postSyncData.getOperation());
    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, postSyncData.getAtomicOpcode());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, GivenComputeWalker2AndInterruptFenceWhencallingSetupPostSyncForInOrderExecThenCorrectValuesAreSet, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ForcePostSyncL1Flush.set(0);
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    WalkerType walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, pDevice->getMemoryManager());

    auto inOrderExecInfo = InOrderExecInfo::create(deviceTagAllocator.getTag(), nullptr, *pDevice, 1);

    dispatchArgs.postSyncArgs.inOrderExecInfo = inOrderExecInfo.get();
    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.shouldAllocateUserFenceReturnSuccess = true;

    inOrderExecInfo->setupInterruptFence();

    auto &postSyncArgs = dispatchArgs.postSyncArgs;
    postSyncArgs.interruptEvent = true;
    EncodePostSync<FamilyType>::template setupPostSyncForInOrderExec<WalkerType>(walkerCmd, postSyncArgs);

    EXPECT_TRUE(walkerCmd.getPostSync().getInterruptSignalEnable());
    auto &postSyncData1 = walkerCmd.getPostSyncOpn1();
    EXPECT_FALSE(postSyncData1.getInterruptSignalEnable());
    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData1.getOperation());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, GivenComputeWalker2WithVariousAtomicDeviceSignallingAndHostStorageOptionsWhencallingSetupPostSyncForInOrderExecThenCorrectValuesAreSet, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    WalkerType walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, pDevice->getMemoryManager());
    MockTagAllocator<DeviceAllocNodeType<true>> hostTagAllocator(0, pDevice->getMemoryManager());

    for (bool hostStorageDuplicated : {true, false}) {
        for (bool atomicSignalling : {true, false}) {
            InOrderExecInfo inOrderExecInfo(deviceTagAllocator.getTag(), (hostStorageDuplicated) ? hostTagAllocator.getTag() : nullptr, *pDevice, 1, atomicSignalling);
            dispatchArgs.postSyncArgs.inOrderExecInfo = &inOrderExecInfo;
            for (bool interruptEvent : {true, false}) {

                dispatchArgs.postSyncArgs.interruptEvent = interruptEvent;
                EXPECT_EQ(hostStorageDuplicated, inOrderExecInfo.isHostStorageDuplicated());
                EXPECT_EQ(atomicSignalling, inOrderExecInfo.isAtomicDeviceSignalling());
                auto &postSyncArgs = dispatchArgs.postSyncArgs;
                EncodePostSync<FamilyType>::template setupPostSyncForInOrderExec<WalkerType>(walkerCmd, postSyncArgs);

                auto &postSyncData = walkerCmd.getPostSync();
                EXPECT_EQ(interruptEvent, postSyncData.getInterruptSignalEnable());
                if (atomicSignalling) {
                    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::OPERATION_ATOMIC_OPN, postSyncData.getOperation());
                    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B, postSyncData.getAtomicOpcode());
                } else {
                    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData.getOperation());
                }
                if (hostStorageDuplicated) {
                    auto &postSyncData1 = walkerCmd.getPostSyncOpn1();
                    EXPECT_FALSE(postSyncData1.getInterruptSignalEnable());
                    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData1.getOperation());
                }
            }
        }
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, GivenComputeWalker2AndDefaultArgsWhencallingAdjustTimestampPacketThenCorrectValuesAreSet, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);

    WalkerType walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();

    auto &postSyncArgs = dispatchArgs.postSyncArgs;

    postSyncArgs.interruptEvent = false;
    EncodePostSync<FamilyType>::template adjustTimestampPacket<WalkerType>(walkerCmd, postSyncArgs);
    EXPECT_FALSE(walkerCmd.getPostSync().getInterruptSignalEnable());

    postSyncArgs.interruptEvent = true;
    EncodePostSync<FamilyType>::template adjustTimestampPacket<WalkerType>(walkerCmd, postSyncArgs);
    EXPECT_TRUE(walkerCmd.getPostSync().getInterruptSignalEnable());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenUsingCsrHeapWithoutScratchNorIndirectDataPtrWhenEncodeComputeWalker2ThenInlineDataIsNotProgrammed, IsAtLeastXe3pCore) {
    using COMPUTE_WALKER_2 = typename FamilyType::COMPUTE_WALKER_2;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 1024u;
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;

    uint64_t inlineFillPattern = 0xABABABABABABABAB;
    uint8_t *crossThreadData = dispatchInterface->dataCrossThread;
    std::fill(crossThreadData, crossThreadData + dispatchInterface->getCrossThreadDataSize(), static_cast<uint8_t>(inlineFillPattern));

    auto &ultCsr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (ultCsr.globalStatelessHeapAllocation) {
        ultCsr.releaseGlobalStatelessHeap();
    }

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.device->getDefaultEngine().commandStreamReceiver = &ultCsr;
    auto csr = dispatchArgs.device->getDefaultEngine().commandStreamReceiver;
    auto ssh = &csr->getIndirectHeap(NEO::surfaceState, 0);
    dispatchArgs.isHeaplessModeEnabled = true;
    dispatchArgs.immediateScratchAddressPatching = true;
    dispatchArgs.surfaceStateHeap = ssh;

    cmdContainer->setImmediateCmdListCsr(csr);

    struct TestParam {
        uint8_t scratchPointerSize;
        CrossThreadDataOffset scratchOffset;
        uint8_t indirectDataPointerSize;
        InlineDataOffset indirectDataOffset;
    };

    std::vector<TestParam> testParams = {
        {undefined<uint8_t>, 0u, undefined<uint8_t>, 8u},
        {8u, undefined<CrossThreadDataOffset>, 8u, undefined<InlineDataOffset>}};

    for (const auto &testParam : testParams) {
        dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = testParam.scratchPointerSize;
        dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset = testParam.scratchOffset;
        dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = testParam.indirectDataPointerSize;
        dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset = testParam.indirectDataOffset;

        EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER_2>(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

        auto itor = find<COMPUTE_WALKER_2 *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<COMPUTE_WALKER_2 *>(*itor);
        auto inlineData = reinterpret_cast<uint64_t *>(walkerCmd->getInlineDataPointer());

        auto indirectDataInInline = inlineData[0];
        auto scratchPointerInInline = inlineData[1];

        EXPECT_EQ(indirectDataInInline, inlineFillPattern);
        EXPECT_EQ(scratchPointerInInline, inlineFillPattern);
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenIndirectDataWhenEncodeComputeWalker2ThenInlineDataContainCorrectIndirectDataPointer, IsAtLeastXe3pCore) {
    using COMPUTE_WALKER_2 = typename FamilyType::COMPUTE_WALKER_2;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0u;
    dispatchInterface->kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = 8u;

    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.isHeaplessModeEnabled = true;

    EncodeDispatchKernel<FamilyType>::template encode<COMPUTE_WALKER_2>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<COMPUTE_WALKER_2 *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<COMPUTE_WALKER_2 *>(*itor);
    auto inlineData = reinterpret_cast<uint64_t *>(walkerCmd->getInlineDataPointer());

    auto indirectHeap = cmdContainer->getIndirectHeap(HeapType::indirectObject);
    auto expectedAddress = indirectHeap->getHeapGpuBase() + indirectHeap->getHeapGpuStartOffset();

    auto indirectDataPointerProgrammed = inlineData[0];

    EXPECT_EQ(expectedAddress, indirectDataPointerProgrammed);
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenForceL1P5CacheForRenderSurfaceFlagWhenSetL1P5CacheIsCalledThenApplyL1P5Appropriately, IsAtLeastXe3pCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    DebugManagerStateRestore restore;

    RENDER_SURFACE_STATE surfaceState = FamilyType::cmdInitRenderSurfaceState;
    for (auto forceL1P5CacheForRenderSurface : {false, true}) {
        debugManager.flags.ForceL1P5CacheForRenderSurface.set(forceL1P5CacheForRenderSurface);
        EncodeSurfaceState<FamilyType>::setAdditionalCacheSettings(&surfaceState);
        EXPECT_EQ(!forceL1P5CacheForRenderSurface, surfaceState.getDisableL1P5());
    }
}

using CommandEncoderTestXe3pAndLater = ::testing::Test;

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenUseSemaphore64bCmdArgWhenProgrammingSemaphoreThenProperSemaphoreIsProgrammed, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT_LEGACY = typename FamilyType::MI_SEMAPHORE_WAIT_LEGACY;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool useSemaphore64bCmd = false;

    // MI_SEMAPHORE_WAIT_LEGACY must be compatible in size with MI_SEMAPHORE_WAIT_64
    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    auto semaphoreLegacy = genCmdCast<MI_SEMAPHORE_WAIT_LEGACY *>(buffer);
    EXPECT_NE(semaphoreLegacy, nullptr);

    useSemaphore64bCmd = true;
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(buffer);
    EXPECT_NE(semaphore, nullptr);
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenForceSwitchQueueOnUnsuccessfulFlagWhenProgrammingSemaphoreLegacyThenSetSwitchOnUnsuccessfulSwitchMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT_LEGACY = typename FamilyType::MI_SEMAPHORE_WAIT_LEGACY;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    bool useSemaphore64bCmd = false;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(buffer);

    {
        bool switchOnUnsuccessful = false;
        debugManager.flags.ForceSwitchQueueOnUnsuccessful.set(1);
        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, switchOnUnsuccessful, useSemaphore64bCmd, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT_LEGACY::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
    }

    linearStream.replaceBuffer(buffer, sizeof(buffer));

    {
        bool switchOnUnsuccessful = true;
        debugManager.flags.ForceSwitchQueueOnUnsuccessful.set(0);
        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, switchOnUnsuccessful, useSemaphore64bCmd, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT_LEGACY::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());
    }
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenForceSwitchQueueOnUnsuccessfulFlagWhenProgrammingSemaphore64ThenSetSwitchOnUnsuccessfulSwitchMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    bool useSemaphore64bCmd = true;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);
    LinearStream linearStream(buffer, sizeof(buffer));
    {
        bool switchOnUnsuccessful = false;
        debugManager.flags.ForceSwitchQueueOnUnsuccessful.set(1);
        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, switchOnUnsuccessful, useSemaphore64bCmd, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
    }

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    {
        bool switchOnUnsuccessful = true;
        debugManager.flags.ForceSwitchQueueOnUnsuccessful.set(0);
        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, switchOnUnsuccessful, useSemaphore64bCmd, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());
    }
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, whenProgrammingSemaphoreLegacyThenSetSwitchOnUnsuccessfulSwitchMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT_LEGACY = typename FamilyType::MI_SEMAPHORE_WAIT_LEGACY;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool useSemaphore64bCmd = false;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(buffer);

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_EQ(MI_SEMAPHORE_WAIT_LEGACY::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, true, useSemaphore64bCmd, nullptr);
    EXPECT_EQ(MI_SEMAPHORE_WAIT_LEGACY::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, whenProgrammingSemaphore64ThenSetSwitchOnUnsuccessfulSwitchMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool useSemaphore64bCmd = true;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());
    EXPECT_TRUE(semaphoreCmd->getCommandControlledInhibitContextSwitch());
    EXPECT_TRUE(semaphoreCmd->getSemaphoreInterrupt());

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, true, useSemaphore64bCmd, nullptr);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
    EXPECT_TRUE(semaphoreCmd->getCommandControlledInhibitContextSwitch());
    EXPECT_TRUE(semaphoreCmd->getSemaphoreInterrupt());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenOverrideCommandControlledInhibitContextSwitchWhenProgrammingSemaphore64ThenSetRequestedValue, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    bool useSemaphore64bCmd = true;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);
    LinearStream linearStream(buffer, sizeof(buffer));

    debugManager.flags.OverrideCommandControlledInhibitContextSwitch.set(0);
    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->getCommandControlledInhibitContextSwitch());

    debugManager.flags.OverrideCommandControlledInhibitContextSwitch.set(1);
    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_TRUE(semaphoreCmd->getCommandControlledInhibitContextSwitch());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenOverrideSemaphoreInterruptWhenProgrammingSemaphore64ThenSetRequestedValue, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    bool useSemaphore64bCmd = true;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);
    LinearStream linearStream(buffer, sizeof(buffer));

    debugManager.flags.OverrideSemaphoreInterrupt.set(0);
    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->getSemaphoreInterrupt());

    debugManager.flags.OverrideSemaphoreInterrupt.set(1);
    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_TRUE(semaphoreCmd->getSemaphoreInterrupt());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenOverrideFastModePollWhenProgrammingSemaphore64ThenSetRequestedValue, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    bool useSemaphore64bCmd = true;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);
    LinearStream linearStream(buffer, sizeof(buffer));

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->getFastModePoll());

    debugManager.flags.OverrideFastModePoll.set(0);
    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->getFastModePoll());

    debugManager.flags.OverrideFastModePoll.set(1);
    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_TRUE(semaphoreCmd->getFastModePoll());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenIndirectModeAndQwordDataWhenProgrammingSemaphoreLegacyThenEnable64bGprMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT_LEGACY = typename FamilyType::MI_SEMAPHORE_WAIT_LEGACY;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool useSemaphore64bCmd = false;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(buffer);

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64BCompareEnableWithGpr());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, true, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64BCompareEnableWithGpr());
    EXPECT_TRUE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, true, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64BCompareEnableWithGpr());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, true, true, false, useSemaphore64bCmd, nullptr);
    EXPECT_TRUE(semaphoreCmd->get64BCompareEnableWithGpr());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenIndirectModeAndQwordDataWhenProgrammingSemaphore64ThenEnable64bGprMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT_64 = typename FamilyType::MI_SEMAPHORE_WAIT_64;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool useSemaphore64bCmd = true;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT_64)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_64 *>(buffer);

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_TRUE(semaphoreCmd->get64BCompareDisable());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, true, false, useSemaphore64bCmd, nullptr);
    EXPECT_TRUE(semaphoreCmd->get64BCompareDisable());
    EXPECT_TRUE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, true, false, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64BCompareDisable());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(buffer));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, true, true, false, useSemaphore64bCmd, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64BCompareDisable());
    EXPECT_TRUE(semaphoreCmd->getIndirectSemaphoreDataDword());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, GivenMiSemaphoreWait64WhenProgrammingWithSelectedWaitModeThenProperWaitModeIsSet, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT_64 = typename FamilyType::MI_SEMAPHORE_WAIT_64;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool useSemaphore64bCmd = true;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT_64)] = {};
    auto miSemaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);
    auto miSemaphore64 = reinterpret_cast<MI_SEMAPHORE_WAIT_64 *>(buffer);

    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(miSemaphore,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false,
                                                        true,
                                                        false,
                                                        false,
                                                        false,
                                                        useSemaphore64bCmd);

    EXPECT_EQ(MI_SEMAPHORE_WAIT_64::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphore64->getCompareOperation());
    EXPECT_EQ(4u, miSemaphore64->getSemaphoreDataDword());
    EXPECT_EQ(0x123400u, miSemaphore64->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT_64::WAIT_MODE::WAIT_MODE_POLLING_MODE, miSemaphore64->getWaitMode());

    memset(buffer, 0, sizeof(buffer));
    EXPECT_ANY_THROW(EncodeSemaphore<FamilyType>::programMiSemaphoreWait(miSemaphore,
                                                                         0x123400,
                                                                         4,
                                                                         MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                                         false,
                                                                         false,
                                                                         false,
                                                                         false,
                                                                         false,
                                                                         useSemaphore64bCmd));
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, GivenMiSemaphoreWaitLegacyWhenProgrammingWithSelectedWaitModeThenProperWaitModeIsSet, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT_LEGACY = typename FamilyType::MI_SEMAPHORE_WAIT_LEGACY;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    bool useSemaphore64bCmd = false;

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    auto miSemaphoreLegacy = reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(buffer);
    auto miSemaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(miSemaphore,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false,
                                                        true,
                                                        false,
                                                        false,
                                                        false,
                                                        useSemaphore64bCmd);

    EXPECT_EQ(MI_SEMAPHORE_WAIT_LEGACY::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphoreLegacy->getCompareOperation());
    EXPECT_EQ(4u, miSemaphoreLegacy->getSemaphoreDataDword());
    EXPECT_EQ(0x123400u, miSemaphoreLegacy->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT_LEGACY::WAIT_MODE::WAIT_MODE_POLLING_MODE, miSemaphoreLegacy->getWaitMode());

    memset(buffer, 0, sizeof(buffer));
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(miSemaphore,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false,
                                                        false,
                                                        false,
                                                        false,
                                                        false,
                                                        useSemaphore64bCmd);
    EXPECT_EQ(MI_SEMAPHORE_WAIT_LEGACY::WAIT_MODE::WAIT_MODE_SIGNAL_MODE, miSemaphoreLegacy->getWaitMode());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, WhenGettingContextTimestampRegisterOffsetsThenQueueTimestampRegisterIsReturned, IsAtLeastXe3pCore) {
    EXPECT_EQ(RegisterOffsets::queueTimestampRegAddressOffsetHigh, ContextTimestampRegister<FamilyType>::getRegisterOffsetHigh());
    EXPECT_EQ(RegisterOffsets::queueTimestampRegAddressOffsetLow, ContextTimestampRegister<FamilyType>::getRegisterOffsetLow());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenPipelinedEuThreadArbitrationPolicyWhenEncodeEuSchedulingPolicyIsCalledThenIddContainsCorrectEuSchedulingPolicy, IsAtLeastXe3pCore) {
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    KernelDescriptor kernelDescriptor;
    int32_t defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;

    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }

    defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;

    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenOverrideThreadArbitrationPolicyDebugFlagSetWhenEncodeEuSchedulingPolicyIsCalledThenIddContainsOverriddenValue, IsAtLeastXe3pCore) {
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    DebugManagerStateRestore debugRestorer;

    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    KernelDescriptor kernelDescriptor;
    kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
    int32_t defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;

    {
        debugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        debugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::AgeBased);
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, idd.getEuThreadSchedulingModeOverride());
    }
    {
        debugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobinAfterDependency);
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        debugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
}

using MemorySynchronizationCommandsTestXe3pAndLater = ::testing::Test;

HWTEST2_F(MemorySynchronizationCommandsTestXe3pAndLater, givenLinearStreamWhenSingleBarrierIsProgrammedThenOnlyCurrentQueueIsDrainedByDefaultAndAllQueuesAreDrainedWithDebugKey, IsAtLeastXe3pCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    auto pc = reinterpret_cast<PIPE_CONTROL *>(buffer);

    PipeControlArgs args{};
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    DebugManagerStateRestore restore;
    debugManager.flags.PcQueueDrainMode.set(0);

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, args);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));
}

HWTEST2_F(MemorySynchronizationCommandsTestXe3pAndLater, givenStallingBarrierWhenProgrammedThenOnlyCurrentQueueIsDrainedByDefaultAndDebugKeyControlsDrainScope, IsAtLeastXe3pCore) {
    using RESOURCE_BARRIER = typename FamilyType::RESOURCE_BARRIER;
    uint32_t buffer[2 * sizeof(RESOURCE_BARRIER)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    auto resourceBarrier = reinterpret_cast<RESOURCE_BARRIER *>(buffer);

    PipeControlArgs args{};
    args.csStallOnly = true;

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, resourceBarrier->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, resourceBarrier->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    DebugManagerStateRestore restore;
    debugManager.flags.PcQueueDrainMode.set(0);

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, resourceBarrier->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, resourceBarrier->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    debugManager.flags.PcQueueDrainMode.set(1);

    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, resourceBarrier->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(buffer, PostSyncMode::noWrite, 0, 0, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, resourceBarrier->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));
}

HWTEST2_F(MemorySynchronizationCommandsTestXe3pAndLater, givenDrainAllQueuesEnabledAndCacheInvalidationWhenSingleBarrierIsProgrammedThenAllQueuesAreDrained, IsAtLeastXe3pCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto pc = reinterpret_cast<PIPE_CONTROL *>(buffer);

    DebugManagerStateRestore restore;
    debugManager.flags.DrainAllQueuesOnCacheInvalidation.set(1);

    bool PipeControlArgs::*const invalidationFlags[] = {
        &PipeControlArgs::instructionCacheInvalidateEnable,
        &PipeControlArgs::stateCacheInvalidationEnable,
        &PipeControlArgs::textureCacheInvalidationEnable,
        &PipeControlArgs::constantCacheInvalidationEnable,
        &PipeControlArgs::tlbInvalidation,
    };

    for (auto flag : invalidationFlags) {
        PipeControlArgs args{};
        args.*flag = true;
        MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
        EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
        linearStream.replaceBuffer(buffer, sizeof(buffer));
    }
}

HWTEST2_F(MemorySynchronizationCommandsTestXe3pAndLater, givenDrainAllQueuesEnabledAndCacheInvalidationForcedByDebugKeyWhenSingleBarrierIsProgrammedThenAllQueuesAreDrained, IsAtLeastXe3pCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto pc = reinterpret_cast<PIPE_CONTROL *>(buffer);

    const int32_t invalidationMasks[] = {
        FlushCachesBitmask::instructionCache,
        FlushCachesBitmask::textureCache,
        FlushCachesBitmask::constantCache,
        FlushCachesBitmask::stateCache,
        FlushCachesBitmask::tlb,
    };

    DebugManagerStateRestore restore;
    debugManager.flags.DrainAllQueuesOnCacheInvalidation.set(1);

    for (auto mask : invalidationMasks) {
        debugManager.flags.FlushAllCaches.set(mask);

        PipeControlArgs args{};
        MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
        EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
        linearStream.replaceBuffer(buffer, sizeof(buffer));
    }
}

HWTEST2_F(MemorySynchronizationCommandsTestXe3pAndLater, givenDrainAllQueuesOnCacheInvalidationDisabledWhenCacheIsInvalidatedThenOnlyCurrentQueueIsDrained, IsAtLeastXe3pCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto pc = reinterpret_cast<PIPE_CONTROL *>(buffer);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    DebugManagerStateRestore restore;
    debugManager.flags.DrainAllQueuesOnCacheInvalidation.set(0);

    PipeControlArgs args{};
    args.tlbInvalidation = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    debugManager.flags.FlushAllCaches.set(FlushCachesBitmask::allCaches);

    PipeControlArgs argsWithoutInvalidation{};
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, argsWithoutInvalidation);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::addStateCacheFlush(linearStream, rootDeviceEnvironment);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
}

HWTEST2_F(MemorySynchronizationCommandsTestXe3pAndLater, givenDrainAllQueuesOnCacheInvalidationEnabledWhenCacheIsInvalidatedThenAllQueuesAreDrained, IsAtLeastXe3pCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto pc = reinterpret_cast<PIPE_CONTROL *>(buffer);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    DebugManagerStateRestore restore;
    debugManager.flags.DrainAllQueuesOnCacheInvalidation.set(1);

    PipeControlArgs args{};
    args.tlbInvalidation = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    MemorySynchronizationCommands<FamilyType>::addStateCacheFlush(linearStream, rootDeviceEnvironment);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
}

HWTEST2_F(MemorySynchronizationCommandsTestXe3pAndLater, givenNonTriggeringCacheFlagsWhenSingleBarrierIsProgrammedThenOnlyCurrentQueueIsDrained, IsAtLeastXe3pCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto pc = reinterpret_cast<PIPE_CONTROL *>(buffer);

    PipeControlArgs args{};
    args.dcFlushEnable = true;
    args.renderTargetCacheFlushEnable = true;
    args.vfCacheInvalidationEnable = true;
    MemorySynchronizationCommands<FamilyType>::addSingleBarrier(linearStream, args);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
}

HWTEST2_F(MemorySynchronizationCommandsTestXe3pAndLater, givenStateCacheFlushWhenProgrammedThenAllQueuesAreDrainedByDefaultAndDebugKeysControlDrainScope, IsAtLeastXe3pCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    uint32_t buffer[2 * sizeof(PIPE_CONTROL)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto pc = reinterpret_cast<PIPE_CONTROL *>(buffer);

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    MemorySynchronizationCommands<FamilyType>::addStateCacheFlush(linearStream, rootDeviceEnvironment);
    EXPECT_TRUE(pc->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pc->getTextureCacheInvalidationEnable());
    EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    DebugManagerStateRestore restore;
    debugManager.flags.DrainAllQueuesOnCacheInvalidation.set(1);

    MemorySynchronizationCommands<FamilyType>::addStateCacheFlush(linearStream, rootDeviceEnvironment);
    EXPECT_EQ(QueueDrainMode::drainAllQueues, pc->getQueueDrainMode());
    linearStream.replaceBuffer(buffer, sizeof(buffer));

    debugManager.flags.PcQueueDrainMode.set(1);

    MemorySynchronizationCommands<FamilyType>::addStateCacheFlush(linearStream, rootDeviceEnvironment);
    EXPECT_EQ(QueueDrainMode::drainOnlyCurrentQueue, pc->getQueueDrainMode());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenMidThreadPreemptionAndHostWaitablePostSyncWhenEncodingWalkerThenThreadPreemptionIsSetAccordingToProductHelper, IsXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto &productHelper = pDevice->getProductHelper();

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, pDevice->getMemoryManager());
    auto inOrderExecInfo = InOrderExecInfo::create(deviceTagAllocator.getTag(), nullptr, *pDevice, 1);

    for (bool inOrderExec : {true, false}) {
        for (bool hostScopeSignalEvent : {true, false}) {
            uint32_t dims[] = {1, 1, 1};
            std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
            EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
            dispatchArgs.preemptionMode = PreemptionMode::MidThread;
            dispatchArgs.postSyncArgs.inOrderExecInfo = inOrderExec ? inOrderExecInfo.get() : nullptr;
            dispatchArgs.postSyncArgs.isHostScopeSignalEvent = hostScopeSignalEvent;
            dispatchArgs.postSyncArgs.eventAddress = 0x1000;
            dispatchArgs.postSyncArgs.eventPacketsCount = 1;

            EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);

            auto walkerCmd = reinterpret_cast<WalkerType *>(dispatchArgs.outWalkerPtr);
            ASSERT_NE(nullptr, walkerCmd);

            const bool hostWaitablePostSync = inOrderExec || hostScopeSignalEvent;
            const bool downgradeRequired = productHelper.isWalkerPreemptionFallbackRequired(PreemptionMode::MidThread, hostWaitablePostSync);
            EXPECT_EQ(!downgradeRequired, walkerCmd->getInterfaceDescriptor().getThreadPreemption());
        }
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenThreadGroupPreemptionAndHostWaitablePostSyncWhenEncodingWalkerThenThreadPreemptionIsDisabled, IsXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, false);
    dispatchArgs.preemptionMode = PreemptionMode::ThreadGroup;
    dispatchArgs.postSyncArgs.isHostScopeSignalEvent = true;
    dispatchArgs.postSyncArgs.eventAddress = 0x1000;
    dispatchArgs.postSyncArgs.eventPacketsCount = 1;

    EncodeDispatchKernel<FamilyType>::template encode<WalkerType>(*cmdContainer.get(), dispatchArgs);

    auto walkerCmd = reinterpret_cast<WalkerType *>(dispatchArgs.outWalkerPtr);
    ASSERT_NE(nullptr, walkerCmd);
    EXPECT_FALSE(walkerCmd->getInterfaceDescriptor().getThreadPreemption());
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, givenProductHelperWithInvalidGrfNumbersWhenProgrammingIddThenErrorIsThrown, IsAtLeastXe3pCore) {
    struct ProductHelperWithInvalidGrfNumbers : public NEO::ProductHelperHw<IGFX_UNKNOWN> {
        const SupportedNumGrfs getSupportedNumGrfs(const NEO::ReleaseHelper &releaseHelper) const override {
            return invalidGrfs;
        }
        SupportedNumGrfs invalidGrfs = {127u, 255u};
    };

    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    MockExecutionEnvironment mockExecutionEnvironment{};
    RAIIProductHelperFactory<ProductHelperWithInvalidGrfNumbers> productHelperBackup{*mockExecutionEnvironment.rootDeviceEnvironments[0]};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0].get();

    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    size_t emptyValue = 0;

    std::vector<uint32_t> supportedNumGrfs = {128, 256};

    for (auto &numGrf : supportedNumGrfs) {
        EXPECT_THROW(EncodeDispatchKernel<FamilyType>::setGrfInfo(&idd, numGrf, emptyValue, emptyValue, rootDeviceEnvironment),
                     std::exception);
    }
}

HWTEST2_F(CommandEncoderTestXe3pAndLater, given57bitVaForDestinationAddressWhenProgrammingMiFlushDwThenVerifyAll57bitsAreUsed, IsAtLeastXe3pCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    MockExecutionEnvironment mockExecutionEnvironment{};
    const uint64_t setGpuAddress = 0xffffffffffffffff;
    const uint64_t verifyGpuAddress = 0xfffffffffffffff8;

    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, setGpuAddress, 0, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    EXPECT_EQ(verifyGpuAddress, miFlushDwCmd->getDestinationAddress());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenEncodeSurfaceStateAndFlagEnableExtendedScratchSurfaceSizeDisabledWhenSetPitchForScratchThenPitchIsCorrect, IsAtLeastXe3pCore) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t pitch = 128u;
    const auto &productHelper = getHelper<ProductHelper>();
    EncodeSurfaceState<FamilyType>::setPitchForScratch(&surfaceState, pitch, productHelper);
    auto pitchFromEncode = EncodeSurfaceState<FamilyType>::getPitchForScratchInBytes(&surfaceState, productHelper);
    EXPECT_EQ(pitch, pitchFromEncode);
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenDisabledFlagEnableExtendedScratchSurfaceSizeWhenCallGetPitchForScratchInBytesThenPitchIsAsSet, IsAtLeastXe3pCore) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t pitch = 64u;
    surfaceState.setSurfacePitch(pitch);
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(pitch, EncodeSurfaceState<FamilyType>::getPitchForScratchInBytes(&surfaceState, productHelper));
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenDisabledFlagEnableExtendedScratchSurfaceSizeWhenCallSetPitchForScratchThenPitchIsAsSetAndExtendScratchSurfaceSizeIsNotSet, IsAtLeastXe3pCore) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t pitch = 128u;
    const auto &productHelper = getHelper<ProductHelper>();
    EncodeSurfaceState<FamilyType>::setPitchForScratch(&surfaceState, pitch, productHelper);
    EXPECT_EQ(pitch, surfaceState.getSurfacePitch());
    EXPECT_FALSE(surfaceState.getExtendScratchSurfaceSize());
}

using Walker2DispatchTestsXe3pAndLater = ::testing::Test;

template <typename WalkerType, typename InterfaceDescriptorDataType, typename FamilyType>
static void whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySetFunction() {
    DebugManagerStateRestore debugRestorer;
    MockExecutionEnvironment executionEnvironment;
    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();

    EncodeWalkerArgs walkerArgs{
        .kernelExecutionType = KernelExecutionType::concurrent,
        .requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none,
        .maxFrontEndThreads = 113,
        .requiredSystemFence = true,
        .hasSample = false};

    {
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    VariableBackup<uint32_t> sliceCountBackup(&executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.SliceCount, 4);

    {
        walkerArgs.kernelExecutionType = KernelExecutionType::defaultType;
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        walkerCmd.getInterfaceDescriptor().setThreadGroupDispatchSize(InterfaceDescriptorDataType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        VariableBackup<uint32_t> sliceCountBackup(&executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.SliceCount, 2);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        EncodeDispatchKernel<FamilyType>::template encodeComputeDispatchAllWalker<WalkerType, InterfaceDescriptorDataType>(walkerCmd, nullptr, *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        VariableBackup<uint32_t> maxFrontEndThreadsBackup(&walkerArgs.maxFrontEndThreads, 0u);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        walkerCmd.getInterfaceDescriptor().setThreadGroupDispatchSize(InterfaceDescriptorDataType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.set(1);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }
}

HWTEST2_F(Walker2DispatchTestsXe3pAndLater, whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet, IsAtLeastXe3pCore) {
    whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySetFunction<typename FamilyType::DefaultWalkerType, typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2, FamilyType>();
}

template <typename WalkerType, typename FamilyType>
static void givenSampleSetWhenEncodingExtraParamsThenSetCorrectFieldsFunction() {

    using DISPATCH_WALK_ORDER = typename WalkerType::DISPATCH_WALK_ORDER;
    using THREAD_GROUP_BATCH_SIZE = typename WalkerType::THREAD_GROUP_BATCH_SIZE;

    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs = CommandEncodeStatesFixture::createDefaultEncodeWalkerArgs(kernelDescriptor);

    {
        walkerArgs.hasSample = false;
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_NE(DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK, walkerCmd.getDispatchWalkOrder());
        EXPECT_EQ(THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_1, walkerCmd.getThreadGroupBatchSize());
    }

    {
        walkerArgs.hasSample = true;
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_EQ(DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK, walkerCmd.getDispatchWalkOrder());
        EXPECT_EQ(THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_4, walkerCmd.getThreadGroupBatchSize());
    }
}

HWTEST2_F(Walker2DispatchTestsXe3pAndLater, givenSampleSetWhenEncodingExtraParamsThenSetCorrectFields, IsAtLeastXe3pCore) {
    givenSampleSetWhenEncodingExtraParamsThenSetCorrectFieldsFunction<typename FamilyType::DefaultWalkerType, FamilyType>();
}

HWTEST2_F(Walker2DispatchTestsXe3pAndLater, givenMaximumNumberOfThreadsWhenEncodingExtraParamsThenSetCorrectFields, IsAtLeastXe3pCore) {

    DebugManagerStateRestore restore;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    KernelDescriptor kernelDescriptor;
    uint32_t maximumNumberOfThreads = 64u;
    EncodeWalkerArgs walkerArgs = CommandEncodeStatesFixture::createDefaultEncodeWalkerArgs(kernelDescriptor);
    walkerArgs.maxFrontEndThreads = maximumNumberOfThreads;

    EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
    EXPECT_EQ(maximumNumberOfThreads, walkerCmd.getMaximumNumberOfThreads());

    uint32_t maximumNumberOfThreadsOverride = 32u;
    debugManager.flags.MaximumNumberOfThreads.set(static_cast<int32_t>(maximumNumberOfThreadsOverride));
    EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
    EXPECT_EQ(maximumNumberOfThreadsOverride, walkerCmd.getMaximumNumberOfThreads());
}

HWTEST2_F(Walker2DispatchTestsXe3pAndLater, givenDebugFlagSetWhenProgrammingAdditionalWalkerFieldsThenSetThreadArbitrationPolicy, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;
    using THREAD_ARBITRATION_POLICY = typename WalkerType::THREAD_ARBITRATION_POLICY;

    DebugManagerStateRestore restore;

    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA_2>();

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);

    EXPECT_EQ(THREAD_ARBITRATION_POLICY::THREAD_ARBITRATION_POLICY_ALWAYS_ROUND_ROBIN, walkerCmd.getThreadArbitrationPolicy());

    THREAD_ARBITRATION_POLICY expectedValue = THREAD_ARBITRATION_POLICY::THREAD_ARBITRATION_POLICY_ABRITRATION_SWITCH_AT_50;
    debugManager.flags.OverrideComputeWalker2ThreadArbitrationPolicy.set(static_cast<int32_t>(expectedValue));

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
    EXPECT_EQ(expectedValue, walkerCmd.getThreadArbitrationPolicy());
}

HWTEST2_F(Walker2DispatchTestsXe3pAndLater, givenDebugFlagSetWhenProgrammingAdditionalWalkerFieldsThenSetThreadDispatchPolicy, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;
    using THREAD_DISPATCH_POLICY = typename WalkerType::THREAD_DISPATCH_POLICY;

    DebugManagerStateRestore restore;

    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA_2>();

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);

    EXPECT_EQ(THREAD_DISPATCH_POLICY::THREAD_DISPATCH_POLICY_BREADTH_WISE, walkerCmd.getThreadDispatchPolicy());

    debugManager.flags.OverrideComputeWalker2ThreadDispatchPolicy.set(static_cast<int32_t>(THREAD_DISPATCH_POLICY::THREAD_DISPATCH_POLICY_DEPTH_WISE));

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
    EXPECT_EQ(THREAD_DISPATCH_POLICY::THREAD_DISPATCH_POLICY_DEPTH_WISE, walkerCmd.getThreadDispatchPolicy());

    debugManager.flags.OverrideComputeWalker2ThreadDispatchPolicy.set(static_cast<int32_t>(THREAD_DISPATCH_POLICY::THREAD_DISPATCH_POLICY_BREADTH_WISE));

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
    EXPECT_EQ(THREAD_DISPATCH_POLICY::THREAD_DISPATCH_POLICY_BREADTH_WISE, walkerCmd.getThreadDispatchPolicy());
}

HWTEST2_F(Walker2DispatchTestsXe3pAndLater, givenOverDispatchControlDebugFlagWhenProgrammingAdditionalWalkerFieldsThenOverDispatchControlIsSetCorrectly, IsAtLeastXe3pCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    DebugManagerStateRestore restore;
    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA_2>();

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
    EXPECT_EQ(WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_NORMAL, walkerCmd.getOverDispatchControl());

    for (auto overDispatchControl : {WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_NONE,
                                     WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_LOW,
                                     WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_NORMAL,
                                     WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_HIGH}) {

        debugManager.flags.OverDispatchControl.set(overDispatchControl);
        EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
        EXPECT_EQ(overDispatchControl, walkerCmd.getOverDispatchControl());
    }
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenHeaplessModeEnabledWhenPatchScratchAddressInImplicitArgsIsCalledThenScratchIsPatchedCorrectly, IsAtLeastXe3pCore) {
    {
        ImplicitArgs implicitArgs{};
        implicitArgs.v1.header.structVersion = 1;
        implicitArgs.v1.scratchPtr = 0u;
        uint64_t scratchAddress = 0x80;

        bool scratchPtrPatchingRequired = false;
        EncodeDispatchKernel<FamilyType>::patchScratchAddressInImplicitArgs(implicitArgs, scratchAddress, scratchPtrPatchingRequired);

        EXPECT_NE(scratchAddress, implicitArgs.v1.scratchPtr);
    }
    {
        ImplicitArgs implicitArgs{};
        implicitArgs.v1.header.structVersion = 1;
        implicitArgs.v1.scratchPtr = 0u;
        uint64_t scratchAddress = 0x80;

        bool scratchPtrPatchingRequired = true;
        EncodeDispatchKernel<FamilyType>::patchScratchAddressInImplicitArgs(implicitArgs, scratchAddress, scratchPtrPatchingRequired);

        EXPECT_EQ(scratchAddress, implicitArgs.v1.scratchPtr);
    }
}
