/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;

using CommandEncodeStatesTestXe3pAndLater = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, GivenVariousSlmTotalSizesWhenSetPreferredSlmIsCalledThenCorrectValuesAreSet, IsAtLeastXe3pCore) {

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_16K},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K},
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_64K},
        {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_96K},
        {128 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K},
        {160 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_160K},
        {192 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_192K},
    };
    if (this->pDevice->getHardwareInfo().capabilityTable.maxProgrammableSlmSize == 384) {
        valuesToTest.push_back({256 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_256K});
        valuesToTest.push_back({320 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_320K});
        valuesToTest.push_back({384 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_384K});
    }

    verifyPreferredSlmValues<FamilyType>(valuesToTest, this->pDevice->getRootDeviceEnvironment());
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenDebugFlagSetWhenProgrammingSemaphoreSectionThenSetSwitchMode, IsAtLeastXe3pCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using QUEUE_SWITCH_MODE = typename MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE;

    DebugManagerStateRestore restore;

    MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>> directSubmission(*pDevice->getDefaultEngine().commandStreamReceiver);

    bool ret = directSubmission.initialize(false);
    EXPECT_TRUE(ret);

    auto &cmdStream = directSubmission.ringCommandStream;
    auto offset = cmdStream.getUsed();

    {
        GenCmdList cmdList;

        directSubmission.dispatchSemaphoreSection(1u);

        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), offset), (cmdStream.getUsed() - offset)));

        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphore);

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());
    }

    offset = cmdStream.getUsed();

    {
        debugManager.flags.DirectSubmissionSwitchSemaphoreMode.set(1);
        GenCmdList cmdList;

        directSubmission.dispatchSemaphoreSection(1u);

        EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream.getCpuBase(), offset), (cmdStream.getUsed() - offset)));

        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), semaphore);

        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*semaphore);
        ASSERT_NE(nullptr, semaphoreCmd);

        EXPECT_EQ(QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
    }

    offset = cmdStream.getUsed();

    {
        debugManager.flags.DirectSubmissionSwitchSemaphoreMode.set(0);
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
    auto iohDiffAligned = alignUp(iohDiff, NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment());

    EXPECT_EQ(iohDiffAligned, iohDiff);
}

HWTEST2_F(CommandEncodeStatesTestXe3pAndLater, givenDebugFlagSetWhenLSCSamplerBackingThresholdThenCorrectValueIsSet, IsAtLeastXe3pCore) {
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
    auto inOrderExecInfo = InOrderExecInfo::create(deviceTagAllocator.getTag(), nullptr, *pDevice, 1, false);

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

    auto inOrderExecInfo = InOrderExecInfo::create(deviceTagAllocator.getTag(), nullptr, *pDevice, 1, false);

    dispatchArgs.postSyncArgs.inOrderExecInfo = inOrderExecInfo.get();
    auto &postSyncArgs = dispatchArgs.postSyncArgs;
    EncodePostSync<FamilyType>::template setupPostSyncForInOrderExec<WalkerType>(walkerCmd, postSyncArgs);

    auto &postSyncData = walkerCmd.getPostSync();
    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::OPERATION_ATOMIC_OPN, postSyncData.getOperation());
    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_INC8B, postSyncData.getAtomicOpcode());
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
            InOrderExecInfo inOrderExecInfo(deviceTagAllocator.getTag(), (hostStorageDuplicated) ? hostTagAllocator.getTag() : nullptr, *pDevice, 1, false, atomicSignalling);
            dispatchArgs.postSyncArgs.inOrderExecInfo = &inOrderExecInfo;
            for (bool interruptEvent : {true, false}) {

                dispatchArgs.postSyncArgs.interruptEvent = interruptEvent;
                EXPECT_EQ(hostStorageDuplicated, inOrderExecInfo.isHostStorageDuplicated());
                EXPECT_EQ(atomicSignalling, inOrderExecInfo.isAtomicDeviceSignalling());
                auto &postSyncArgs = dispatchArgs.postSyncArgs;
                EncodePostSync<FamilyType>::template setupPostSyncForInOrderExec<WalkerType>(walkerCmd, postSyncArgs);

                auto &postSyncData = walkerCmd.getPostSync();
                if (atomicSignalling) {
                    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::OPERATION_ATOMIC_OPN, postSyncData.getOperation());
                    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_INC8B, postSyncData.getAtomicOpcode());
                } else {
                    EXPECT_EQ(FamilyType::POSTSYNC_DATA_2::OPERATION_WRITE_IMMEDIATE_DATA, postSyncData.getOperation());
                }
                if (hostStorageDuplicated) {
                    auto &postSyncData1 = walkerCmd.getPostSyncOpn1();
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
        pDevice->getMemoryManager()->freeGraphicsMemory(ultCsr.globalStatelessHeapAllocation);
        ultCsr.globalStatelessHeapAllocation = nullptr;
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
        InlineDataOffset scratchOffset;
        uint8_t indirectDataPointerSize;
        InlineDataOffset indirectDataOffset;
    };

    std::vector<TestParam> testParams = {
        {undefined<uint8_t>, 0u, undefined<uint8_t>, 8u},
        {8u, undefined<InlineDataOffset>, 8u, undefined<InlineDataOffset>}};

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
