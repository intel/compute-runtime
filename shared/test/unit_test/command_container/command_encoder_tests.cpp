/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "encode_surface_state_args.h"

using namespace NEO;
using CommandEncoderTests = ::testing::Test;

#include "shared/test/common/test_macros/header/heapless_matchers.h"

HWTEST2_F(CommandEncoderTests, whenPushBindingTableAndSurfaceStatesIsCalledAndHeaplessRequiredThenFail, IsHeaplessRequired) {
    std::byte mockHeap[sizeof(IndirectHeap)];
    EXPECT_ANY_THROW(EncodeSurfaceState<FamilyType>::pushBindingTableAndSurfaceStates(reinterpret_cast<IndirectHeap &>(mockHeap), nullptr, 0, 0, 0));
}

HWTEST_F(CommandEncoderTests, givenMisalignedSizeWhenProgrammingSurfaceStateForBufferThenAlignedSizeIsProgrammed) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    RENDER_SURFACE_STATE renderSurfaceState{};
    MockExecutionEnvironment executionEnvironment{};

    auto misalignedSize = 1u;
    auto alignedSize = 4u;

    NEO::EncodeSurfaceStateArgs args{};
    args.outMemory = &renderSurfaceState;
    args.gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    args.size = misalignedSize;

    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(alignedSize, renderSurfaceState.getWidth());
}

HWTEST_F(CommandEncoderTests, givenDifferentInputParamsWhenCreatingStandaloneInOrderExecInfoThenSetupCorrectly) {
    MockDevice mockDevice;

    uint64_t counterValue = 2;
    uint64_t *hostAddress = &counterValue;
    uint64_t gpuAddress = castToUint64(ptrOffset(&counterValue, 64));

    MockGraphicsAllocation deviceAlloc(nullptr, gpuAddress, 1);

    auto inOrderExecInfo = InOrderExecInfo::createFromExternalAllocation(mockDevice, &deviceAlloc, gpuAddress, nullptr, hostAddress, counterValue, 2, 3);

    EXPECT_EQ(counterValue, inOrderExecInfo->getCounterValue());
    EXPECT_EQ(hostAddress, inOrderExecInfo->getBaseHostAddress());
    EXPECT_EQ(gpuAddress, inOrderExecInfo->getBaseDeviceAddress());
    EXPECT_EQ(&deviceAlloc, inOrderExecInfo->getDeviceCounterAllocation());
    EXPECT_EQ(nullptr, inOrderExecInfo->getHostCounterAllocation());
    EXPECT_TRUE(inOrderExecInfo->isExternalMemoryExecInfo());
    EXPECT_EQ(2u, inOrderExecInfo->getNumDevicePartitionsToWait());
    EXPECT_EQ(3u, inOrderExecInfo->getNumHostPartitionsToWait());
    EXPECT_EQ(0u, inOrderExecInfo->getDeviceNodeWriteSize());
    EXPECT_EQ(0u, inOrderExecInfo->getHostNodeWriteSize());
    EXPECT_EQ(0u, inOrderExecInfo->getDeviceNodeGpuAddress());
    EXPECT_EQ(0u, inOrderExecInfo->getHostNodeGpuAddress());

    inOrderExecInfo->reset();

    EXPECT_EQ(0u, inOrderExecInfo->getCounterValue());

    MockGraphicsAllocation hostAlloc(nullptr, castToUint64(hostAddress), 1);

    inOrderExecInfo = InOrderExecInfo::createFromExternalAllocation(mockDevice, &deviceAlloc, gpuAddress, &hostAlloc, hostAddress, counterValue, 2, 3);
    EXPECT_EQ(&hostAlloc, inOrderExecInfo->getHostCounterAllocation());
}

HWTEST_F(CommandEncoderTests, givenTsNodesWhenStoringOnTempListThenHandleOwnershipCorrectly) {
    class MyMockInOrderExecInfo : public NEO::InOrderExecInfo {
      public:
        using InOrderExecInfo::InOrderExecInfo;
        using InOrderExecInfo::lastWaitedCounterValue;
        using InOrderExecInfo::tempTimestampNodes;
    };

    MockDevice mockDevice;

    using AllocatorT = MockTagAllocator<NEO::TimestampPackets<uint64_t, 1>>;

    AllocatorT tsAllocator(0, mockDevice.getMemoryManager());

    auto node0 = static_cast<AllocatorT::NodeType *>(tsAllocator.getTag());
    auto node1 = static_cast<AllocatorT::NodeType *>(tsAllocator.getTag());

    EXPECT_FALSE(tsAllocator.freeTags.peekContains(*node0));
    EXPECT_FALSE(tsAllocator.freeTags.peekContains(*node1));

    {
        MyMockInOrderExecInfo inOrderExecInfo(nullptr, nullptr, mockDevice, 1, false, false);

        inOrderExecInfo.lastWaitedCounterValue = 0;

        inOrderExecInfo.pushTempTimestampNode(node0, 1);
        inOrderExecInfo.pushTempTimestampNode(node1, 2);

        EXPECT_EQ(2u, inOrderExecInfo.tempTimestampNodes.size());

        inOrderExecInfo.releaseNotUsedTempTimestampNodes(false);
        EXPECT_EQ(2u, inOrderExecInfo.tempTimestampNodes.size());

        EXPECT_FALSE(tsAllocator.freeTags.peekContains(*node0));
        EXPECT_FALSE(tsAllocator.freeTags.peekContains(*node1));

        inOrderExecInfo.lastWaitedCounterValue = 1;
        inOrderExecInfo.releaseNotUsedTempTimestampNodes(false);
        EXPECT_EQ(1u, inOrderExecInfo.tempTimestampNodes.size());
        EXPECT_EQ(node1, inOrderExecInfo.tempTimestampNodes[0].first);

        EXPECT_TRUE(tsAllocator.freeTags.peekContains(*node0));
        EXPECT_FALSE(tsAllocator.freeTags.peekContains(*node1));

        inOrderExecInfo.lastWaitedCounterValue = 2;
        inOrderExecInfo.releaseNotUsedTempTimestampNodes(false);
        EXPECT_EQ(0u, inOrderExecInfo.tempTimestampNodes.size());
        EXPECT_TRUE(tsAllocator.freeTags.peekContains(*node0));
        EXPECT_TRUE(tsAllocator.freeTags.peekContains(*node1));

        node0 = static_cast<AllocatorT::NodeType *>(tsAllocator.getTag());
        node1 = static_cast<AllocatorT::NodeType *>(tsAllocator.getTag());

        EXPECT_FALSE(tsAllocator.freeTags.peekContains(*node0));
        EXPECT_FALSE(tsAllocator.freeTags.peekContains(*node1));

        inOrderExecInfo.pushTempTimestampNode(node0, 3);
        inOrderExecInfo.pushTempTimestampNode(node1, 4);
    }

    // forced release on destruction
    EXPECT_TRUE(tsAllocator.freeTags.peekContains(*node0));
    EXPECT_TRUE(tsAllocator.freeTags.peekContains(*node1));
}

HWTEST_F(CommandEncoderTests, givenDifferentInputParamsWhenCreatingInOrderExecInfoThenSetupCorrectly) {
    MockDevice mockDevice;

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, mockDevice.getMemoryManager());
    MockTagAllocator<DeviceAllocNodeType<true>> hostTagAllocator(0, mockDevice.getMemoryManager());

    auto tempNode1 = deviceTagAllocator.getTag();
    auto tempNode2 = hostTagAllocator.getTag();

    {
        auto deviceNode = deviceTagAllocator.getTag();

        EXPECT_NE(deviceNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()->getGpuAddress(), deviceNode->getGpuAddress());
        EXPECT_NE(deviceNode->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()->getUnderlyingBuffer(), deviceNode->getCpuBase());

        auto inOrderExecInfo = InOrderExecInfo::create(deviceNode, nullptr, mockDevice, 2, false);

        EXPECT_EQ(deviceNode->getCpuBase(), inOrderExecInfo->getBaseHostAddress());
        EXPECT_EQ(deviceNode->getBaseGraphicsAllocation()->getGraphicsAllocation(0), inOrderExecInfo->getDeviceCounterAllocation());
        EXPECT_EQ(deviceNode->getGpuAddress(), inOrderExecInfo->getBaseDeviceAddress());
        EXPECT_EQ(nullptr, inOrderExecInfo->getHostCounterAllocation());
        EXPECT_FALSE(inOrderExecInfo->isHostStorageDuplicated());
        EXPECT_FALSE(inOrderExecInfo->isRegularCmdList());

        auto heaplessEnabled = mockDevice.getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo);

        if (heaplessEnabled) {
            EXPECT_TRUE(inOrderExecInfo->isAtomicDeviceSignalling());
            EXPECT_EQ(1u, inOrderExecInfo->getNumDevicePartitionsToWait());
        } else {
            EXPECT_FALSE(inOrderExecInfo->isAtomicDeviceSignalling());
            EXPECT_EQ(2u, inOrderExecInfo->getNumDevicePartitionsToWait());
        }

        EXPECT_EQ(2u, inOrderExecInfo->getNumHostPartitionsToWait());
        EXPECT_EQ(0u, InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo));
        EXPECT_FALSE(inOrderExecInfo->isExternalMemoryExecInfo());
    }

    {
        auto deviceNode = deviceTagAllocator.getTag();

        InOrderExecInfo inOrderExecInfo(deviceNode, nullptr, mockDevice, 2, true, true);
        EXPECT_TRUE(inOrderExecInfo.isRegularCmdList());
        EXPECT_TRUE(inOrderExecInfo.isAtomicDeviceSignalling());
        EXPECT_EQ(1u, inOrderExecInfo.getNumDevicePartitionsToWait());
        EXPECT_EQ(2u, inOrderExecInfo.getNumHostPartitionsToWait());
    }

    {
        auto deviceNode = deviceTagAllocator.getTag();
        auto hostNode = hostTagAllocator.getTag();
        auto offset = ptrDiff(hostNode->getCpuBase(), tempNode2->getCpuBase());

        DebugManagerStateRestore restore;
        debugManager.flags.InOrderDuplicatedCounterStorageEnabled.set(1);

        constexpr uint32_t partitionCount = 2u;
        auto inOrderExecInfo = InOrderExecInfo::create(deviceNode, hostNode, mockDevice, partitionCount, false);

        EXPECT_EQ(inOrderExecInfo->getBaseHostGpuAddress(), hostNode->getGpuAddress());
        EXPECT_NE(inOrderExecInfo->getDeviceCounterAllocation(), inOrderExecInfo->getHostCounterAllocation());
        EXPECT_NE(deviceNode->getBaseGraphicsAllocation()->getGraphicsAllocation(0), inOrderExecInfo->getHostCounterAllocation());
        EXPECT_NE(0u, inOrderExecInfo->getDeviceNodeGpuAddress());
        size_t deviceNodeSize = sizeof(uint64_t) * (mockDevice.getGfxCoreHelper().inOrderAtomicSignallingEnabled(mockDevice.getRootDeviceEnvironment()) ? 1u : partitionCount);
        EXPECT_EQ(deviceNodeSize, inOrderExecInfo->getDeviceNodeWriteSize());
        EXPECT_NE(0u, inOrderExecInfo->getHostNodeGpuAddress());
        EXPECT_EQ(sizeof(uint64_t) * partitionCount, inOrderExecInfo->getHostNodeWriteSize());

        EXPECT_NE(deviceNode->getCpuBase(), inOrderExecInfo->getBaseHostAddress());
        EXPECT_EQ(ptrOffset(inOrderExecInfo->getHostCounterAllocation()->getUnderlyingBuffer(), offset), inOrderExecInfo->getBaseHostAddress());
        EXPECT_TRUE(inOrderExecInfo->isHostStorageDuplicated());
    }

    {
        auto deviceNode = deviceTagAllocator.getTag();
        auto hostNode = hostTagAllocator.getTag();

        InOrderExecInfo inOrderExecInfo(deviceNode, hostNode, mockDevice, 1, false, false);
        auto deviceAllocHostAddress = reinterpret_cast<uint64_t *>(deviceNode->getCpuBase());
        EXPECT_EQ(0u, inOrderExecInfo.getCounterValue());
        EXPECT_EQ(0u, inOrderExecInfo.getRegularCmdListSubmissionCounter());
        EXPECT_EQ(0u, inOrderExecInfo.getAllocationOffset());
        EXPECT_EQ(0u, *inOrderExecInfo.getBaseHostAddress());
        EXPECT_EQ(0u, *deviceAllocHostAddress);

        inOrderExecInfo.addCounterValue(2);
        inOrderExecInfo.addRegularCmdListSubmissionCounter(3);
        inOrderExecInfo.setAllocationOffset(4);
        *inOrderExecInfo.getBaseHostAddress() = 5;
        *deviceAllocHostAddress = 6;

        EXPECT_EQ(2u, inOrderExecInfo.getCounterValue());
        EXPECT_EQ(3u, inOrderExecInfo.getRegularCmdListSubmissionCounter());
        EXPECT_EQ(4u, inOrderExecInfo.getAllocationOffset());

        inOrderExecInfo.reset();

        EXPECT_EQ(0u, inOrderExecInfo.getCounterValue());
        EXPECT_EQ(0u, inOrderExecInfo.getRegularCmdListSubmissionCounter());
        EXPECT_EQ(0u, inOrderExecInfo.getAllocationOffset());
        EXPECT_EQ(0u, *inOrderExecInfo.getBaseHostAddress());
        EXPECT_EQ(0u, *deviceAllocHostAddress);
    }

    {
        auto deviceNode = deviceTagAllocator.getTag();

        InOrderExecInfo inOrderExecInfo(deviceNode, nullptr, mockDevice, 2, true, false);

        EXPECT_EQ(0u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
        inOrderExecInfo.addCounterValue(2);
        EXPECT_EQ(0u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
        inOrderExecInfo.addRegularCmdListSubmissionCounter(1);
        EXPECT_EQ(0u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
        inOrderExecInfo.addRegularCmdListSubmissionCounter(1);
        EXPECT_EQ(2u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
        inOrderExecInfo.addRegularCmdListSubmissionCounter(1);
        EXPECT_EQ(4u, InOrderPatchCommandHelpers::getAppendCounterValue(inOrderExecInfo));
    }

    tempNode1->returnTag();
    tempNode2->returnTag();
}

HWTEST_F(CommandEncoderTests, givenInOrderExecutionInfoWhenSetLastCounterValueIsCalledThenItReturnsProperQueries) {
    MockDevice mockDevice;

    MockExecutionEnvironment mockExecutionEnvironment{};

    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, mockDevice.getMemoryManager());
    auto node = tagAllocator.getTag();

    auto inOrderExecInfo = std::make_unique<InOrderExecInfo>(node, nullptr, mockDevice, 2, true, false);
    inOrderExecInfo->setLastWaitedCounterValue(1u);

    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(2u));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(1u));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(0u));

    inOrderExecInfo->setLastWaitedCounterValue(0u);
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(2u));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(1u));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(0u));

    inOrderExecInfo->setLastWaitedCounterValue(3u);
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(2u));
    EXPECT_TRUE(inOrderExecInfo->isCounterAlreadyDone(3u));

    inOrderExecInfo->setAllocationOffset(4u);
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(2u));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(3u));
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(0u));

    inOrderExecInfo = std::make_unique<InOrderExecInfo>(nullptr, nullptr, mockDevice, 2, true, false);
    inOrderExecInfo->setLastWaitedCounterValue(2);
    EXPECT_FALSE(inOrderExecInfo->isCounterAlreadyDone(1));
}

HWTEST_F(CommandEncoderTests, givenInOrderExecutionInfoWhenResetCalledThenUploadToTbx) {
    MockDevice mockDevice;
    auto &csr = mockDevice.getUltCommandStreamReceiver<FamilyType>();
    csr.commandStreamReceiverType = CommandStreamReceiverType::tbx;

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, mockDevice.getMemoryManager());
    MockTagAllocator<DeviceAllocNodeType<false>> hostTagAllocator(0, mockDevice.getMemoryManager());
    auto deviceNode = deviceTagAllocator.getTag();
    auto hostNode = hostTagAllocator.getTag();

    EXPECT_EQ(0u, csr.writeMemoryParams.totalCallCount);

    auto inOrderExecInfo = std::make_unique<InOrderExecInfo>(deviceNode, hostNode, mockDevice, 2, true, false);
    EXPECT_EQ(2u, csr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(0u, csr.writeMemoryParams.chunkWriteCallCount);

    inOrderExecInfo->reset();
    EXPECT_EQ(4u, csr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(2u, csr.writeMemoryParams.chunkWriteCallCount);

    inOrderExecInfo = std::make_unique<InOrderExecInfo>(deviceNode, nullptr, mockDevice, 2, true, false);
    EXPECT_EQ(5u, csr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(3u, csr.writeMemoryParams.chunkWriteCallCount);

    inOrderExecInfo->reset();
    EXPECT_EQ(6u, csr.writeMemoryParams.totalCallCount);
    EXPECT_EQ(4u, csr.writeMemoryParams.chunkWriteCallCount);
}

HWTEST_F(CommandEncoderTests, givenInOrderExecInfoWhenPatchingThenSetCorrectValues) {
    MockDevice mockDevice;

    MockExecutionEnvironment mockExecutionEnvironment{};
    MockMemoryManager memoryManager(mockExecutionEnvironment);

    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, mockDevice.getMemoryManager());
    auto node = tagAllocator.getTag();

    auto inOrderExecInfo = std::make_shared<InOrderExecInfo>(node, nullptr, mockDevice, 2, true, false);
    inOrderExecInfo->addCounterValue(1);

    {
        auto cmd = FamilyType::cmdInitStoreDataImm;
        cmd.setDataDword0(1);

        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, reinterpret_cast<void *>(&cmd), nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::sdi, false, false);
        patchCmd.patch(2);

        EXPECT_EQ(3u, cmd.getDataDword0());
    }

    {
        auto cmd = FamilyType::cmdInitMiSemaphoreWait;
        cmd.setSemaphoreDataDword(1);

        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, &cmd, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::semaphore, false, false);
        patchCmd.patch(2);
        EXPECT_EQ(1u, cmd.getSemaphoreDataDword());

        inOrderExecInfo->addRegularCmdListSubmissionCounter(3);
        patchCmd.patch(3);
        EXPECT_EQ(3u, cmd.getSemaphoreDataDword());

        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmdInternal(nullptr, &cmd, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::semaphore, false, false);
        patchCmdInternal.patch(3);

        EXPECT_EQ(4u, cmd.getSemaphoreDataDword());
    }

    {
        auto cmd1 = FamilyType::cmdInitLoadRegisterImm;
        auto cmd2 = FamilyType::cmdInitLoadRegisterImm;
        cmd1.setDataDword(1);
        cmd2.setDataDword(1);

        inOrderExecInfo->reset();
        inOrderExecInfo->addCounterValue(1);
        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, &cmd1, &cmd2, 1, InOrderPatchCommandHelpers::PatchCmdType::lri64b, false, false);
        patchCmd.patch(3);
        EXPECT_EQ(1u, cmd1.getDataDword());
        EXPECT_EQ(1u, cmd2.getDataDword());

        inOrderExecInfo->addRegularCmdListSubmissionCounter(3);
        patchCmd.patch(3);
        EXPECT_EQ(3u, cmd1.getDataDword());
        EXPECT_EQ(0u, cmd2.getDataDword());

        InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmdInternal(nullptr, &cmd1, &cmd2, 1, InOrderPatchCommandHelpers::PatchCmdType::lri64b, false, false);
        patchCmdInternal.patch(2);

        EXPECT_EQ(3u, cmd1.getDataDword());
        EXPECT_EQ(0u, cmd2.getDataDword());
    }

    InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, nullptr, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::none, false, false);
    EXPECT_ANY_THROW(patchCmd.patch(1));
}

HWTEST_F(CommandEncoderTests, givenInOrderExecInfoWhenPatchingWalkerThenSetCorrectValues) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockDevice mockDevice;

    MockExecutionEnvironment mockExecutionEnvironment{};
    MockMemoryManager memoryManager(mockExecutionEnvironment);

    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, mockDevice.getMemoryManager());
    auto node = tagAllocator.getTag();

    auto inOrderExecInfo = std::make_shared<InOrderExecInfo>(node, nullptr, mockDevice, 2, false, false);

    auto cmd = FamilyType::template getInitGpuWalker<DefaultWalkerType>();

    InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, &cmd, nullptr, 1, InOrderPatchCommandHelpers::PatchCmdType::walker, false, false);

    if constexpr (FamilyType::walkerPostSyncSupport) {
        patchCmd.patch(2);

        EXPECT_EQ(3u, cmd.getPostSync().getImmediateData());
    } else {
        EXPECT_ANY_THROW(patchCmd.patch(2));
    }
}

HWTEST_F(CommandEncoderTests, givenInOrderExecInfoWhenPatchingDisabledThenNoCmdBufferUpdated) {
    MockDevice mockDevice;

    MockExecutionEnvironment mockExecutionEnvironment{};
    MockMemoryManager memoryManager(mockExecutionEnvironment);

    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, mockDevice.getMemoryManager());
    auto node = tagAllocator.getTag();

    auto inOrderExecInfo = std::make_shared<InOrderExecInfo>(node, nullptr, mockDevice, 1, true, false);
    inOrderExecInfo->addRegularCmdListSubmissionCounter(4);
    inOrderExecInfo->addCounterValue(1);

    auto cmd = FamilyType::cmdInitMiSemaphoreWait;
    cmd.setSemaphoreDataDword(1);

    constexpr uint64_t baseCounterValue = 1;
    InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, &cmd, nullptr, baseCounterValue, InOrderPatchCommandHelpers::PatchCmdType::semaphore, false, false);

    patchCmd.setSkipPatching(true);
    patchCmd.patch(2);
    EXPECT_EQ(1u, cmd.getSemaphoreDataDword());

    patchCmd.setSkipPatching(false);
    patchCmd.patch(2);
    EXPECT_EQ(4u, cmd.getSemaphoreDataDword());
}

HWTEST_F(CommandEncoderTests, givenNewInOrderExecInfoWhenChangingInOrderExecInfoThenNewValuePatched) {
    MockDevice mockDevice;

    MockExecutionEnvironment mockExecutionEnvironment{};
    MockMemoryManager memoryManager(mockExecutionEnvironment);

    MockTagAllocator<DeviceAllocNodeType<true>> tagAllocator(0, mockDevice.getMemoryManager());
    auto node = tagAllocator.getTag();

    auto inOrderExecInfo = std::make_shared<InOrderExecInfo>(node, nullptr, mockDevice, 1, true, false);
    inOrderExecInfo->addRegularCmdListSubmissionCounter(4);
    inOrderExecInfo->addCounterValue(1);

    auto cmd = FamilyType::cmdInitMiSemaphoreWait;
    cmd.setSemaphoreDataDword(1);

    constexpr uint64_t baseCounterValue = 1;
    InOrderPatchCommandHelpers::PatchCmd<FamilyType> patchCmd(&inOrderExecInfo, &cmd, nullptr, baseCounterValue, InOrderPatchCommandHelpers::PatchCmdType::semaphore, false, false);

    patchCmd.patch(2);
    EXPECT_EQ(4u, cmd.getSemaphoreDataDword());

    auto node2 = tagAllocator.getTag();
    auto inOrderExecInfo2 = std::make_shared<InOrderExecInfo>(node2, nullptr, mockDevice, 1, true, false);
    inOrderExecInfo2->addRegularCmdListSubmissionCounter(6);
    inOrderExecInfo2->addCounterValue(1);

    patchCmd.updateInOrderExecInfo(&inOrderExecInfo2);
    patchCmd.patch(2);
    EXPECT_EQ(6u, cmd.getSemaphoreDataDword());
}

HWTEST_F(CommandEncoderTests, givenImmDataWriteWhenProgrammingMiFlushDwThenSetAllRequiredFields) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    uint64_t gpuAddress = 0x1230000;
    uint64_t immData = 456;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, gpuAddress, immData, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    unsigned int sizeMultiplier = 1;
    if (UnitTestHelper<FamilyType>::additionalMiFlushDwRequired) {
        sizeMultiplier = 2;
        uint64_t gpuAddress = 0x0;
        uint64_t immData = 0;

        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, miFlushDwCmd->getPostSyncOperation());
        EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
        EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
        miFlushDwCmd++;
    }

    EXPECT_EQ(sizeMultiplier * sizeof(MI_FLUSH_DW), linearStream.getUsed());
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushDwCmd->getPostSyncOperation());
    EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
    EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
    EXPECT_EQ(0u, static_cast<uint32_t>(miFlushDwCmd->getNotifyEnable()));
}

HWTEST2_F(CommandEncoderTests, given57bitVaForDestinationAddressWhenProgrammingMiFlushDwThenVerifyAll57bitsAreUsed, IsPVC) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    const uint64_t setGpuAddress = 0xffffffffffffffff;
    const uint64_t verifyGpuAddress = 0xfffffffffffffff8;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, setGpuAddress, 0, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    EXPECT_EQ(verifyGpuAddress, miFlushDwCmd->getDestinationAddress());
}

HWTEST_F(CommandEncoderTests, whenEncodeMemoryPrefetchCalledThenDoNothing) {

    MockExecutionEnvironment mockExecutionEnvironment{};

    uint8_t buffer[MemoryConstants::pageSize] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    GraphicsAllocation allocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 123, 456, 789, MemoryPool::localMemory, MemoryManager::maxOsContextCount);

    EncodeMemoryPrefetch<FamilyType>::programMemoryPrefetch(linearStream, allocation, 2, 0, *mockExecutionEnvironment.rootDeviceEnvironments[0]);

    EXPECT_EQ(0u, linearStream.getUsed());
    EXPECT_EQ(0u, EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(2, *mockExecutionEnvironment.rootDeviceEnvironments[0]));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncoderTests, WhenAnyParameterIsProvidedThenRuntimeGenerationLocalIdsIsRequired) {
    uint32_t workDim = 1;
    uint32_t simd = 8;
    size_t lws[3] = {16, 1, 1};
    std::array<uint8_t, 3> walkOrder = {};
    uint32_t requiredWalkOrder = 0u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, true, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, GivenWorkDimensionsAreZeroWhenAnyParameterIsProvidedThenVerifyRuntimeGenerationLocalIdsIsNotRequired) {
    uint32_t workDim = 0;
    uint32_t simd = 8;
    std::array<uint8_t, 3> walkOrder = {};
    uint32_t requiredWalkOrder = 0u;

    EXPECT_FALSE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, nullptr, walkOrder, false, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, GivenSimdSizeIsOneWhenAnyParameterIsProvidedThenVerifyRuntimeGenerationLocalIdsIsRequired) {
    uint32_t workDim = 3;
    uint32_t simd = 1;
    size_t lws[3] = {7, 7, 5};
    std::array<uint8_t, 3> walkOrder = {0, 1, 2};
    uint32_t requiredWalkOrder = 0u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, false, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, GivenhwGenerationOfLocalIdsDebugFlagIsDisabledWhenAnyParameterIsProvidedThenVerifyRuntimeGenerationLocalIdsIsRequired) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHwGenerationLocalIds.set(0);

    uint32_t workDim = 3;
    uint32_t simd = 8;
    size_t lws[3] = {7, 7, 5};
    std::array<uint8_t, 3> walkOrder = {0, 1, 2};
    uint32_t requiredWalkOrder = 0u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws, walkOrder, false, requiredWalkOrder, simd));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, givenLocalWorkgroupSizeGreaterThen1024ThenRuntimeMustGenerateLocalIds) {
    uint32_t workDim = 3;
    uint32_t simd = 8;
    std::array<size_t, 3> lws = {1025, 1, 1};

    std::array<uint8_t, 3> walkOrder = {{0, 1, 2}};
    uint32_t requiredWalkOrder = 77u;

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));

    lws = {1, 1, 1025};

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));

    lws = {32, 32, 4};

    EXPECT_TRUE(EncodeDispatchKernel<FamilyType>::isRuntimeLocalIdsGenerationRequired(
        workDim, lws.data(), walkOrder, false, requiredWalkOrder, simd));
}

HWTEST_F(CommandEncoderTests, givenNotify) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    uint64_t gpuAddress = 0x1230000;
    uint64_t immData = 456;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;
    args.notifyEnable = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, gpuAddress, immData, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    unsigned int sizeMultiplier = 1;
    if (UnitTestHelper<FamilyType>::additionalMiFlushDwRequired) {
        sizeMultiplier = 2;
        uint64_t gpuAddress = 0x0;
        uint64_t immData = 0;

        EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE, miFlushDwCmd->getPostSyncOperation());
        EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
        EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
        miFlushDwCmd++;
    }

    EXPECT_EQ(sizeMultiplier * sizeof(MI_FLUSH_DW), linearStream.getUsed());
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushDwCmd->getPostSyncOperation());
    EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
    EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
    EXPECT_EQ(1u, static_cast<uint32_t>(miFlushDwCmd->getNotifyEnable()));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncoderTests, whenAppendParamsForImageFromBufferThenNothingChanges) {
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    auto expectedState = surfaceState;

    EXPECT_EQ(0, memcmp(&expectedState, &surfaceState, sizeof(surfaceState)));
    EncodeSurfaceState<FamilyType>::appendParamsForImageFromBuffer(&surfaceState);

    EXPECT_EQ(0, memcmp(&expectedState, &surfaceState, sizeof(surfaceState)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, givenDebugFlagSetWhenProgrammingMiArbThenSetPreparserDisabledValue) {
    DebugManagerStateRestore restore;

    using MI_ARB_CHECK = typename FamilyType::MI_ARB_CHECK;

    alignas(4) uint8_t arbCheckBuffer[sizeof(MI_ARB_CHECK)];
    void *arbCheckBufferPtr = arbCheckBuffer;

    for (int32_t value : {-1, 0, 1}) {
        debugManager.flags.ForcePreParserEnabledForMiArbCheck.set(value);

        memset(arbCheckBuffer, 0, sizeof(arbCheckBuffer));

        MI_ARB_CHECK buffer[2] = {};
        LinearStream linearStream(buffer, sizeof(buffer));
        MockExecutionEnvironment executionEnvironment{};
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
        rootDeviceEnvironment.initGmm();

        EncodeMiArbCheck<FamilyType>::program(linearStream, false);
        EncodeMiArbCheck<FamilyType>::program(arbCheckBufferPtr, false);

        if (value == 0) {
            EXPECT_TRUE(buffer[0].getPreParserDisable());
        } else {
            EXPECT_FALSE(buffer[0].getPreParserDisable());
        }
        EXPECT_EQ(0, memcmp(arbCheckBufferPtr, &buffer[0], sizeof(MI_ARB_CHECK)));
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncoderTests, givenPreXeHpPlatformWhenSetupPostSyncMocsThenNothingHappen) {
    MockExecutionEnvironment executionEnvironment{};
    uint32_t mocs = EncodePostSync<FamilyType>::getPostSyncMocs(*executionEnvironment.rootDeviceEnvironments[0], false);
    EXPECT_EQ(0u, mocs);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncoderTests, givenPreXeHpPlatformWhenCallingAdjustTimestampPacketThenNothingHappen) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    DefaultWalkerType walkerCmd{};
    DefaultWalkerType walkerOnStart{};

    EncodePostSyncArgs args = {.isTimestampEvent = true};
    EncodePostSync<FamilyType>::template adjustTimestampPacket<DefaultWalkerType>(walkerCmd, args);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncoderTests, givenPreXeHpPlatformWhenCallingEncodeL3FlushThenNothingHappen) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    DefaultWalkerType walkerCmd{};
    DefaultWalkerType walkerOnStart{};

    EncodePostSyncArgs args = {.isFlushL3ForExternalAllocationRequired = true};
    EncodePostSync<FamilyType>::template encodeL3Flush<DefaultWalkerType>(walkerCmd, args);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change

    args = {.isFlushL3ForHostUsmRequired = true};
    EncodePostSync<FamilyType>::template encodeL3Flush<DefaultWalkerType>(walkerCmd, args);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncoderTests, givenPreXeHpPlatformWhenCallingSetupPostSyncForRegularEventThenNothingHappen) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    DefaultWalkerType walkerCmd{};
    DefaultWalkerType walkerOnStart{};

    EncodePostSyncArgs args = {.eventAddress = 0x1234};
    EncodePostSync<FamilyType>::template setupPostSyncForRegularEvent<DefaultWalkerType>(walkerCmd, args);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncoderTests, givenPreXeHpPlatformWhenCallingSetupPostSyncForInOrderExecThenNothingHappen) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    MockDevice mockDevice;
    DefaultWalkerType walkerCmd{};
    DefaultWalkerType walkerOnStart{};

    MockTagAllocator<DeviceAllocNodeType<true>> deviceTagAllocator(0, mockDevice.getMemoryManager());
    auto deviceNode = deviceTagAllocator.getTag();

    InOrderExecInfo inOrderExecInfo(deviceNode, nullptr, mockDevice, 2, true, true);
    EncodePostSyncArgs args = {.inOrderExecInfo = &inOrderExecInfo};
    EncodePostSync<FamilyType>::template setupPostSyncForInOrderExec<DefaultWalkerType>(walkerCmd, args);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, givenAtLeastXeHpPlatformWhenSetupPostSyncMocsThenCorrect) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.initGmm();
    bool dcFlush = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, rootDeviceEnvironment);

    {
        DefaultWalkerType walkerCmd{};
        uint32_t mocs = 0;
        EXPECT_NO_THROW(mocs = EncodePostSync<FamilyType>::getPostSyncMocs(*executionEnvironment.rootDeviceEnvironments[0], dcFlush));
        EXPECT_NO_THROW(walkerCmd.getPostSync().setMocs(mocs));

        auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
        auto expectedMocs = dcFlush ? gmmHelper->getUncachedMOCS() : gmmHelper->getL3EnabledMOCS();

        EXPECT_EQ(expectedMocs, walkerCmd.getPostSync().getMocs());
    }
    {
        DebugManagerStateRestore restorer{};
        auto expectedMocs = 9u;
        debugManager.flags.OverridePostSyncMocs.set(expectedMocs);
        DefaultWalkerType walkerCmd{};
        uint32_t mocs = 0;
        EXPECT_NO_THROW(mocs = EncodePostSync<FamilyType>::getPostSyncMocs(*executionEnvironment.rootDeviceEnvironments[0], false));
        EXPECT_NO_THROW(walkerCmd.getPostSync().setMocs(mocs));
        EXPECT_EQ(expectedMocs, walkerCmd.getPostSync().getMocs());
    }
}

HWTEST2_F(CommandEncoderTests, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenWalkerIsNotChanged, IsAtMostXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    DefaultWalkerType walkerCmd{};
    DefaultWalkerType walkerOnStart{};

    uint32_t yOrder = 2u;
    EncodeDispatchKernel<FamilyType>::template adjustWalkOrder<DefaultWalkerType>(walkerCmd, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change

    uint32_t linearOrder = 0u;
    EncodeDispatchKernel<FamilyType>::template adjustWalkOrder<DefaultWalkerType>(walkerCmd, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change

    uint32_t fakeOrder = 5u;
    EncodeDispatchKernel<FamilyType>::template adjustWalkOrder<DefaultWalkerType>(walkerCmd, fakeOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change
}

HWTEST_F(CommandEncoderTests, givenDcFlushNotRequiredWhenGettingDcFlushValueThenReturnValueIsFalse) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    constexpr bool requiredFlag = false;
    bool helperValue = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(requiredFlag, rootDeviceEnvironment);
    EXPECT_FALSE(helperValue);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncoderTests, givenXeHpPlatformsWhenGettingDefaultSshSizeThenExpectTwoMegabytes) {
    constexpr size_t expectedSize = 2 * MemoryConstants::megaByte;
    EXPECT_EQ(expectedSize, EncodeStates<FamilyType>::getSshHeapSize());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, CommandEncoderTests, givenPreXeHpPlatformsWhenGettingDefaultSshSizeThenExpectSixtyFourKilobytes) {
    constexpr size_t expectedSize = 64 * MemoryConstants::kiloByte;
    EXPECT_EQ(expectedSize, EncodeStates<FamilyType>::getSshHeapSize());
}

HWTEST2_F(CommandEncoderTests, whenUsingDefaultFilteringAndAppendSamplerStateParamsThenDisableLowQualityFilter, MatchAny) {
    EXPECT_FALSE(debugManager.flags.ForceSamplerLowFilteringPrecision.get());
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getProductHelper();

    auto state = FamilyType::cmdInitSamplerState;

    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    productHelper.adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}

HWTEST2_F(CommandEncoderTests, givenMiStoreRegisterMemWhenEncodeAndIsBcsThenRegisterOffsetsBcs0Base, MatchAny) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    uint64_t baseAddr = 0x10;
    uint32_t offset = 0x2000;

    constexpr size_t bufferSize = 2100;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    auto buf = cmdStream.getSpaceForCmd<MI_STORE_REGISTER_MEM>();

    bool isBcs = true;
    EncodeStoreMMIO<FamilyType>::encode(buf, offset, baseAddr, true, isBcs);
    auto storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(buffer);
    ASSERT_NE(nullptr, storeRegMem);
    EXPECT_EQ(storeRegMem->getRegisterAddress(), RegisterOffsets::bcs0Base + offset);

    isBcs = false;
    EncodeStoreMMIO<FamilyType>::encode(buf, offset, baseAddr, true, isBcs);
    storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(buffer);
    ASSERT_NE(nullptr, storeRegMem);
    EXPECT_EQ(storeRegMem->getRegisterAddress(), offset);
}

HWTEST2_F(CommandEncoderTests, givenMiLoadRegisterMemWhenEncodememAndIsBcsThenRegisterOffsetsBcs0Base, MatchAny) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    uint64_t baseAddr = 0x10;
    uint32_t offset = 0x2000;

    constexpr size_t bufferSize = 2100;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    uint8_t *ptr = buffer;
    bool isBcs = true;

    EncodeSetMMIO<FamilyType>::encodeMEM(cmdStream, offset, baseAddr, isBcs);
    auto loadRegMem = genCmdCast<MI_LOAD_REGISTER_MEM *>(ptr);
    ASSERT_NE(nullptr, loadRegMem);
    EXPECT_EQ(loadRegMem->getRegisterAddress(), RegisterOffsets::bcs0Base + offset);

    isBcs = false;
    ptr += sizeof(MI_LOAD_REGISTER_MEM);
    EncodeSetMMIO<FamilyType>::encodeMEM(cmdStream, offset, baseAddr, isBcs);
    loadRegMem = genCmdCast<MI_LOAD_REGISTER_MEM *>(ptr);
    ASSERT_NE(nullptr, loadRegMem);
    EXPECT_EQ(loadRegMem->getRegisterAddress(), offset);
}

HWTEST2_F(CommandEncoderTests, givenMiLoadRegisterRegwhenencoderegAndIsBcsThenRegisterOffsetsBcs0Base, MatchAny) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;

    uint32_t srcOffset = 0x2000;
    uint32_t dstOffset = 0x2004;

    constexpr size_t bufferSize = 2100;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);
    uint8_t *ptr = buffer;
    bool isBcs = true;

    EncodeSetMMIO<FamilyType>::encodeREG(cmdStream, dstOffset, srcOffset, isBcs);
    auto storeRegReg = genCmdCast<MI_LOAD_REGISTER_REG *>(buffer);
    ASSERT_NE(nullptr, storeRegReg);
    EXPECT_EQ(storeRegReg->getSourceRegisterAddress(), RegisterOffsets::bcs0Base + srcOffset);
    EXPECT_EQ(storeRegReg->getDestinationRegisterAddress(), RegisterOffsets::bcs0Base + dstOffset);

    isBcs = false;
    ptr += sizeof(MI_LOAD_REGISTER_REG);
    EncodeSetMMIO<FamilyType>::encodeREG(cmdStream, dstOffset, srcOffset, isBcs);
    storeRegReg = genCmdCast<MI_LOAD_REGISTER_REG *>(ptr);
    EXPECT_EQ(storeRegReg->getSourceRegisterAddress(), srcOffset);
    EXPECT_EQ(storeRegReg->getDestinationRegisterAddress(), dstOffset);
}

HWTEST2_F(CommandEncoderTests, whenForcingLowQualityFilteringAndAppendSamplerStateParamsThenEnableLowQualityFilter, MatchAny) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceSamplerLowFilteringPrecision.set(true);
    EXPECT_TRUE(debugManager.flags.ForceSamplerLowFilteringPrecision.get());
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getProductHelper();

    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    auto state = FamilyType::cmdInitSamplerState;

    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    productHelper.adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE, state.getLowQualityFilter());
}

HWTEST2_F(CommandEncoderTests, givenSdiCommandWhenProgrammingThenForceWriteCompletionCheck, MatchAny) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    constexpr size_t bufferSize = sizeof(MI_STORE_DATA_IMM);
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    EncodeStoreMemory<FamilyType>::programStoreDataImm(cmdStream, 0, 0, 0, false, false, nullptr);

    auto storeDataImm = genCmdCast<MI_STORE_DATA_IMM *>(buffer);
    ASSERT_NE(nullptr, storeDataImm);
    EXPECT_TRUE(storeDataImm->getForceWriteCompletionCheck());
}

HWTEST2_F(CommandEncoderTests, whenAskingForImplicitScalingValuesThenAlwaysReturnStubs, IsGen12LP) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.incRefInternal();
    auto rootExecEnv = executionEnvironment.rootDeviceEnvironments[0].get();

    uint8_t buffer[128] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    WalkerType walkerCmd = {};

    DeviceBitfield deviceBitField = 1;
    uint32_t partitionCount = 1;

    Vec3<size_t> vec3 = {1, 1, 1};

    EXPECT_EQ(0u, ImplicitScalingDispatch<FamilyType>::template getSize<WalkerType>(false, false, deviceBitField, vec3, vec3));

    void *ptr = nullptr;

    auto device = std::make_unique<MockDevice>(&executionEnvironment, 0);

    ImplicitScalingDispatchCommandArgs args{
        0,                       // workPartitionAllocationGpuVa
        device.get(),            // device
        &ptr,                    // outWalkerPtr
        RequiredPartitionDim::x, // requiredPartitionDim
        partitionCount,          // partitionCount
        1,                       // workgroupSize
        1,                       // threadGroupCount
        1,                       // maxWgCountPerTile
        false,                   // useSecondaryBatchBuffer
        false,                   // apiSelfCleanup
        false,                   // dcFlush
        false,                   // forceExecutionOnSingleTile
        false,                   // blockDispatchToCommandBuffer
        false};                  // isRequiredDispatchWorkGroupOrder

    ImplicitScalingDispatch<FamilyType>::dispatchCommands(linearStream, walkerCmd, deviceBitField, args);
    EXPECT_EQ(0u, linearStream.getUsed());

    EXPECT_TRUE(ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired());

    EXPECT_EQ(0u, ImplicitScalingDispatch<FamilyType>::getBarrierSize(*rootExecEnv, false, false));

    PipeControlArgs pcArgs = {};
    ImplicitScalingDispatch<FamilyType>::dispatchBarrierCommands(linearStream, deviceBitField, pcArgs, *rootExecEnv, 0, 0, false, false);
    EXPECT_EQ(0u, linearStream.getUsed());

    EXPECT_EQ(0u, ImplicitScalingDispatch<FamilyType>::getRegisterConfigurationSize());

    ImplicitScalingDispatch<FamilyType>::dispatchRegisterConfiguration(linearStream, 0, 0, false);
    EXPECT_EQ(0u, linearStream.getUsed());

    EXPECT_EQ(0u, ImplicitScalingDispatch<FamilyType>::getOffsetRegisterSize());

    ImplicitScalingDispatch<FamilyType>::dispatchOffsetRegister(linearStream, 0, 0);
    EXPECT_EQ(0u, linearStream.getUsed());

    EXPECT_EQ(static_cast<uint32_t>(sizeof(uint64_t)), ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset());

    EXPECT_EQ(static_cast<uint32_t>(GfxCoreHelperHw<FamilyType>::getSingleTimestampPacketSizeHw()), ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset());

    EXPECT_FALSE(ImplicitScalingDispatch<FamilyType>::platformSupportsImplicitScaling(*rootExecEnv));
}

HWTEST2_F(CommandEncoderTests, givenInterfaceDescriptorWhenEncodeEuSchedulingPolicyIsCalledThenChanged, IsAtLeastXe3Core) {
    using INTERFACE_DESCRIPTOR_DATA = typename EncodeDispatchKernel<FamilyType>::INTERFACE_DESCRIPTOR_DATA;

    auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA>();

    KernelDescriptor kernelDescriptor;
    kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
    int32_t defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
    EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);

    EXPECT_EQ(idd.getEuThreadSchedulingModeOverride(), INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST);
}

HWTEST2_F(CommandEncoderTests, givenInterfaceDescriptorWhenEncodeEuSchedulingPolicyIsCalledThenNothingIsChanged, IsAtMostXe2HpgCore) {

    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    auto expectedIdd = idd;

    KernelDescriptor kernelDescriptor;
    kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
    int32_t defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
    EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);

    constexpr uint32_t iddSizeInDW = 8;
    for (uint32_t i = 0u; i < iddSizeInDW; i++) {
        EXPECT_EQ(expectedIdd.getRawData(i), idd.getRawData(i));
    }
}

HWTEST_F(CommandEncoderTests, whenGetScratchPtrOffsetOfImplicitArgsIsCalledThenZeroIsReturned) {

    auto scratchOffset = EncodeDispatchKernel<FamilyType>::getScratchPtrOffsetOfImplicitArgs();
    EXPECT_EQ(0u, scratchOffset);
}
