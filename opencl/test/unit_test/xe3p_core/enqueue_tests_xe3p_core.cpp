/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"
#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

#include <list>
#include <memory>

using namespace NEO;

struct EnqueueFixtureXe3pCore : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableMemoryPrefetch.set(1);

        hwInfo = *defaultHwInfo;
        hwInfo.featureTable.flags.ftrCCSNode = true;
        hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
        hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

        clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, mockRootDeviceIndex));
        context = std::make_unique<MockContext>(clDevice.get());

        mockKernel = std::make_unique<MockKernelWithInternals>(*clDevice, context.get());
        mockKernel->kernelInfo.createKernelAllocation(clDevice->getDevice(), false);

        dispatchInfo = {clDevice.get(), mockKernel->mockKernel, 1, 0, 0, 0};
    }

    void TearDown() override {
        clDevice->getMemoryManager()->freeGraphicsMemory(mockKernel->kernelInfo.getIsaGraphicsAllocation());
    }

    template <typename FamilyType>
    std::unique_ptr<MockCommandQueueHw<FamilyType>> createCommandQueue() {
        return std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), clDevice.get(), nullptr);
    }

    DebugManagerStateRestore restore;
    HardwareInfo hwInfo;
    std::unique_ptr<MockClDevice> clDevice;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockKernelWithInternals> mockKernel;
    DispatchInfo dispatchInfo;
};

using ProgramWalkerTestsXe3pCore = EnqueueFixtureXe3pCore;

using CommandQueueTestsXe3pCore = EnqueueFixtureXe3pCore;

XE3P_CORETEST_F(CommandQueueTestsXe3pCore, whenMultipleComputeCmdQueuesCreatedThenEveryQueueHasDifferentCsrAndOsContextAssigned) {
    auto &productHelper = this->clDevice->getDevice().getProductHelper();
    if (!productHelper.areSecondaryContextsSupported()) {
        GTEST_SKIP();
    }

    auto commandQueue1 = createCommandQueue<FamilyType>();
    auto commandQueue2 = createCommandQueue<FamilyType>();
    auto commandQueue3 = createCommandQueue<FamilyType>();
    auto commandQueue4 = createCommandQueue<FamilyType>();

    auto engine1 = commandQueue1->gpgpuEngine;
    auto engine2 = commandQueue2->gpgpuEngine;
    auto engine3 = commandQueue3->gpgpuEngine;
    auto engine4 = commandQueue4->gpgpuEngine;

    EXPECT_EQ(nullptr, engine1);
    EXPECT_EQ(nullptr, engine2);
    EXPECT_EQ(nullptr, engine3);
    EXPECT_EQ(nullptr, engine4);

    engine1 = &commandQueue1->getGpgpuEngine();
    engine2 = &commandQueue2->getGpgpuEngine();
    engine3 = &commandQueue3->getGpgpuEngine();
    engine4 = &commandQueue4->getGpgpuEngine();

    EXPECT_NE(engine1, engine2);
    EXPECT_NE(engine2, engine3);
    EXPECT_NE(engine3, engine4);
    EXPECT_NE(engine4, engine1);

    EXPECT_NE(engine1->commandStreamReceiver, engine2->commandStreamReceiver);
    EXPECT_NE(engine2->commandStreamReceiver, engine3->commandStreamReceiver);
    EXPECT_NE(engine3->commandStreamReceiver, engine4->commandStreamReceiver);
    EXPECT_NE(engine4->commandStreamReceiver, engine1->commandStreamReceiver);

    EXPECT_NE(engine1->osContext, engine2->osContext);
    EXPECT_NE(engine2->osContext, engine3->osContext);
    EXPECT_NE(engine3->osContext, engine4->osContext);
    EXPECT_NE(engine4->osContext, engine1->osContext);

    EXPECT_TRUE(engine1->osContext->isPartOfContextGroup());
    EXPECT_EQ(nullptr, engine1->osContext->getPrimaryContext());

    EXPECT_TRUE(engine2->osContext->isPartOfContextGroup());
    EXPECT_EQ(engine1->osContext, engine2->osContext->getPrimaryContext());

    EXPECT_TRUE(engine3->osContext->isPartOfContextGroup());
    EXPECT_EQ(engine1->osContext, engine3->osContext->getPrimaryContext());

    EXPECT_TRUE(engine4->osContext->isPartOfContextGroup());
    EXPECT_EQ(engine1->osContext, engine4->osContext->getPrimaryContext());
}

XE3P_CORETEST_F(ProgramWalkerTestsXe3pCore, givenE64EnabledAndTransientL3UnavailableWhenSetupTimestampPacketFlushL3ThenCorrectFlushOptionsAreSetInPostSync) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.Enable64BitAddressing.set(1);
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(1);
    NEO::debugManager.flags.RedirectFlushL3HostUsmToExternal.set(0);

    auto &productHelper = this->clDevice->getDevice().getProductHelper();
    if (!productHelper.isL3FlushAfterPostSyncSupported(true)) {
        GTEST_SKIP();
    }

    auto commandQueue = createCommandQueue<FamilyType>();
    auto &commandStream = commandQueue->getCS(1024);

    auto &heap = commandQueue->getIndirectHeap(IndirectHeap::Type::dynamicState, 1);
    size_t workSize[] = {1, 1, 1};
    Vec3<size_t> wgInfo = {1, 1, 1};

    size_t commandsOffset = 0;

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(workSize, wgInfo, PreemptionMode::Disabled);
    clDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    TimestampPacketContainer timestampPacketContainer;
    timestampPacketContainer.add(clDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    timestampPacketContainer.add(clDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    walkerArgs.currentTimestampPacketNodes = &timestampPacketContainer;
    walkerArgs.blocking = true;
    mockKernel->mockKernel->anyKernelArgumentUsingSystemMemory = true;

    {
        commandsOffset = commandStream.getUsed();

        HardwareInterface<FamilyType>::template programWalker<WalkerType>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                          heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
        auto itorWalker = find<WalkerType *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<WalkerType *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_FALSE(walkerCmd->getPostSync().getL2Flush());
        EXPECT_TRUE(walkerCmd->getPostSync().getL2TransientFlush());

        EXPECT_FALSE(commandQueue->getPendingL3FlushForHostVisibleResources()); // no need to tag kernel since it was already flushed
    }
    {
        NEO::debugManager.flags.RedirectFlushL3HostUsmToExternal.set(1);
        commandsOffset = commandStream.getUsed();

        HardwareInterface<FamilyType>::template programWalker<WalkerType>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                          heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
        auto itorWalker = find<WalkerType *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<WalkerType *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_TRUE(walkerCmd->getPostSync().getL2Flush());
        EXPECT_FALSE(walkerCmd->getPostSync().getL2TransientFlush());
        EXPECT_FALSE(commandQueue->getPendingL3FlushForHostVisibleResources()); // no need to tag kernel since it was already flushed
    }
}

XE3P_CORETEST_F(ProgramWalkerTestsXe3pCore, givenEnableL3FlushAfterPostSyncNonBlockingWithoutEventWhenProgramWalkerThenKernelIsTaggedAndL3FlushIsNotSet) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.Enable64BitAddressing.set(1);
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(1);

    auto &productHelper = this->clDevice->getDevice().getProductHelper();
    if (!productHelper.isL3FlushAfterPostSyncSupported(true)) {
        GTEST_SKIP();
    }

    auto commandQueue = createCommandQueue<FamilyType>();
    auto &commandStream = commandQueue->getCS(1024);

    auto &heap = commandQueue->getIndirectHeap(IndirectHeap::Type::dynamicState, 1);
    size_t workSize[] = {1, 1, 1};
    Vec3<size_t> wgInfo = {1, 1, 1};

    size_t commandsOffset = 0;

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(workSize, wgInfo, PreemptionMode::Disabled);
    clDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    TimestampPacketContainer timestampPacketContainer;
    timestampPacketContainer.add(clDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    timestampPacketContainer.add(clDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    walkerArgs.currentTimestampPacketNodes = &timestampPacketContainer;
    mockKernel->mockKernel->anyKernelArgumentUsingSystemMemory = true;

    ASSERT_FALSE(walkerArgs.blocking);
    ASSERT_TRUE(walkerArgs.event == nullptr);

    {
        NEO::debugManager.flags.RedirectFlushL3HostUsmToExternal.set(0);

        commandsOffset = commandStream.getUsed();

        HardwareInterface<FamilyType>::template programWalker<WalkerType>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                          heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
        auto itorWalker = find<WalkerType *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<WalkerType *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_FALSE(walkerCmd->getPostSync().getL2Flush());
        EXPECT_FALSE(walkerCmd->getPostSync().getL2TransientFlush());

        EXPECT_TRUE(commandQueue->getPendingL3FlushForHostVisibleResources());
    }
    {
        NEO::debugManager.flags.RedirectFlushL3HostUsmToExternal.set(1);

        commandsOffset = commandStream.getUsed();

        HardwareInterface<FamilyType>::template programWalker<WalkerType>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                          heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
        auto itorWalker = find<WalkerType *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<WalkerType *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_FALSE(walkerCmd->getPostSync().getL2Flush());
        EXPECT_FALSE(walkerCmd->getPostSync().getL2TransientFlush());

        EXPECT_TRUE(commandQueue->getPendingL3FlushForHostVisibleResources());
    }
}

XE3P_CORETEST_F(ProgramWalkerTestsXe3pCore, givenForceL3FlushInPostSynchenProgramWalkerIsCalledThenCorrectFlushOptionsAreSetInPostSync) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.Enable64BitAddressing.set(1);
    NEO::debugManager.flags.RedirectFlushL3HostUsmToExternal.set(0);
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(1);

    auto &productHelper = this->clDevice->getDevice().getProductHelper();
    if (!productHelper.isL3FlushAfterPostSyncSupported(true)) {
        GTEST_SKIP();
    }

    auto commandQueue = createCommandQueue<FamilyType>();
    auto &commandStream = commandQueue->getCS(1024);

    auto &heap = commandQueue->getIndirectHeap(IndirectHeap::Type::dynamicState, 1);
    size_t workSize[] = {1, 1, 1};
    Vec3<size_t> wgInfo = {1, 1, 1};

    size_t commandsOffset = 0;

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(workSize, wgInfo, PreemptionMode::Disabled);
    clDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    TimestampPacketContainer timestampPacketContainer;
    timestampPacketContainer.add(clDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    timestampPacketContainer.add(clDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    walkerArgs.currentTimestampPacketNodes = &timestampPacketContainer;

    {

        NEO::debugManager.flags.ForceFlushL3AfterPostSyncForHostUsm.set(true);
        NEO::debugManager.flags.ForceFlushL3AfterPostSyncForExternalAllocation.set(false);

        commandsOffset = commandStream.getUsed();

        HardwareInterface<FamilyType>::template programWalker<WalkerType>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                          heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
        auto itorWalker = find<WalkerType *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<WalkerType *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_FALSE(walkerCmd->getPostSync().getL2Flush());
        EXPECT_TRUE(walkerCmd->getPostSync().getL2TransientFlush());
    }
    {
        NEO::debugManager.flags.ForceFlushL3AfterPostSyncForHostUsm.set(false);
        NEO::debugManager.flags.ForceFlushL3AfterPostSyncForExternalAllocation.set(true);

        commandsOffset = commandStream.getUsed();

        HardwareInterface<FamilyType>::template programWalker<WalkerType>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                          heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
        auto itorWalker = find<WalkerType *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<WalkerType *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_TRUE(walkerCmd->getPostSync().getL2Flush());
        EXPECT_FALSE(walkerCmd->getPostSync().getL2TransientFlush());
    }
}

XE3P_CORETEST_F(ProgramWalkerTestsXe3pCore, givenAllocationsWithCacheFlushRequiredWhenNonBlockingSetupTimestampPacketFlushL3IsCalledThenPendingL3FlushIsSet) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    {

        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = true,
            .signalEvent = false,
            .blocking = true,
            .usingSystemAllocation = true};

        auto commandQueue = createCommandQueue<FamilyType>();
        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_TRUE(postSync.getL2TransientFlush());
        EXPECT_TRUE(postSync.getL2Flush());

        EXPECT_FALSE(commandQueue->getPendingL3FlushForHostVisibleResources());
    }
    {
        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = false,
            .signalEvent = false,
            .blocking = false,
            .usingSystemAllocation = true};

        auto commandQueue = createCommandQueue<FamilyType>();
        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_FALSE(postSync.getL2TransientFlush());
        EXPECT_FALSE(postSync.getL2Flush());

        EXPECT_TRUE(commandQueue->getPendingL3FlushForHostVisibleResources());
    }
    {
        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = true,
            .signalEvent = false,
            .blocking = false,
            .usingSystemAllocation = false};

        auto commandQueue = createCommandQueue<FamilyType>();
        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_FALSE(postSync.getL2TransientFlush());
        EXPECT_FALSE(postSync.getL2Flush());

        EXPECT_TRUE(commandQueue->getPendingL3FlushForHostVisibleResources());
    }
}

XE3P_CORETEST_F(ProgramWalkerTestsXe3pCore, givenMultipleDebugFlagsFlushConfigurationsWhenSetupTimestampPacketFlushL3IsCalledThenPostSyncIsProgrammedCorrectly) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();
    auto commandQueue = createCommandQueue<FamilyType>();

    {
        DebugManagerStateRestore restorer;
        NEO::debugManager.flags.RedirectFlushL3HostUsmToExternal.set(1);

        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = false,
            .signalEvent = false,
            .blocking = true,
            .usingSystemAllocation = true};

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_FALSE(postSync.getL2TransientFlush());
        EXPECT_TRUE(postSync.getL2Flush());
    }
    {
        DebugManagerStateRestore restorer;
        NEO::debugManager.flags.ForceFlushL3AfterPostSyncForExternalAllocation.set(1);

        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = false,
            .signalEvent = false,
            .blocking = false,
            .usingSystemAllocation = false};

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_FALSE(postSync.getL2TransientFlush());
        EXPECT_TRUE(postSync.getL2Flush());
    }
    {
        DebugManagerStateRestore restorer;
        NEO::debugManager.flags.ForceFlushL3AfterPostSyncForHostUsm.set(1);

        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = false,
            .signalEvent = false,
            .blocking = false,
            .usingSystemAllocation = false};

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_TRUE(postSync.getL2TransientFlush());
        EXPECT_FALSE(postSync.getL2Flush());
    }
}

XE3P_CORETEST_F(ProgramWalkerTestsXe3pCore, givenMultipleFlushConfigurationsWhenSetupTimestampPacketFlushL3IsCalledThenPostSyncIsProgrammedCorrectly) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    auto commandQueue = createCommandQueue<FamilyType>();

    {
        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = false,
            .signalEvent = false,
            .blocking = false,
            .usingSystemAllocation = false};
        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_FALSE(postSync.getL2TransientFlush());
        EXPECT_FALSE(postSync.getL2Flush());
    }
    {
        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = true,
            .signalEvent = false,
            .blocking = false,
            .usingSystemAllocation = false};
        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_FALSE(postSync.getL2TransientFlush());
        EXPECT_FALSE(postSync.getL2Flush());
    }
    {
        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = false,
            .signalEvent = false,
            .blocking = false,
            .usingSystemAllocation = true};
        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_FALSE(postSync.getL2TransientFlush());
        EXPECT_FALSE(postSync.getL2Flush());
    }
    {
        FlushL3Args args{
            .containsPrintBuffer = true,
            .usingSharedObjects = false,
            .signalEvent = false,
            .blocking = false,
            .usingSystemAllocation = false};
        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_TRUE(postSync.getL2TransientFlush());
        EXPECT_FALSE(postSync.getL2Flush());
    }
    {
        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = true,
            .signalEvent = true,
            .blocking = false,
            .usingSystemAllocation = false};

        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_FALSE(postSync.getL2TransientFlush());
        EXPECT_TRUE(postSync.getL2Flush());
    }
    {
        FlushL3Args args{
            .containsPrintBuffer = false,
            .usingSharedObjects = true,
            .signalEvent = false,
            .blocking = true,
            .usingSystemAllocation = true};

        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_TRUE(postSync.getL2TransientFlush());
        EXPECT_TRUE(postSync.getL2Flush());
    }
    {
        FlushL3Args args{
            .containsPrintBuffer = true,
            .usingSharedObjects = true,
            .signalEvent = false,
            .blocking = true,
            .usingSystemAllocation = false};

        WalkerType computeWalker = FamilyType::template getInitGpuWalker<WalkerType>();

        GpgpuWalkerHelper<FamilyType>::template setupTimestampPacketFlushL3<WalkerType>(computeWalker, *commandQueue, args);
        auto &postSync = computeWalker.getPostSync();
        EXPECT_TRUE(postSync.getL2TransientFlush());
        EXPECT_TRUE(postSync.getL2Flush());
    }
}

XE3P_CORETEST_F(ProgramWalkerTestsXe3pCore, givenL3FlushAfterPostSyncEnabledAndBlockingKernelWhenProgramWalkerThenFlushOptionsAreSetCorrectly) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(1);

    auto &productHelper = this->clDevice->getDevice().getProductHelper();
    if (!productHelper.isL3FlushAfterPostSyncSupported(true)) {
        GTEST_SKIP();
    }

    auto commandQueue = createCommandQueue<FamilyType>();
    auto &commandStream = commandQueue->getCS(1024);

    auto &heap = commandQueue->getIndirectHeap(IndirectHeap::Type::dynamicState, 1);
    size_t workSize[] = {1, 1, 1};
    Vec3<size_t> wgInfo = {1, 1, 1};

    size_t commandsOffset = 0;

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(workSize, wgInfo, PreemptionMode::Disabled);
    walkerArgs.blocking = true;

    clDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    TimestampPacketContainer timestampPacketContainer;
    timestampPacketContainer.add(clDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    walkerArgs.currentTimestampPacketNodes = &timestampPacketContainer;

    mockKernel->mockKernel->anyKernelArgumentUsingSystemMemory = true;
    commandsOffset = commandStream.getUsed();
    HardwareInterface<FamilyType>::template programWalker<WalkerType>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                      heap, heap, heap, dispatchInfo, walkerArgs);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
    auto itorWalker = find<WalkerType *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    EXPECT_NE(hwParse.cmdList.end(), itorWalker);
    auto walkerCmdParsed = genCmdCast<WalkerType *>(*itorWalker);
    EXPECT_NE(nullptr, walkerCmdParsed);

    auto &postSync = walkerCmdParsed->getPostSync();
    EXPECT_TRUE(postSync.getL2TransientFlush());
    EXPECT_FALSE(postSync.getL2Flush());
}

XE3P_CORETEST_F(ProgramWalkerTestsXe3pCore, givenL3FlushAfterPostSyncEnabledAndPrintfHandlerWhenProgramWalkerThenFlushOptionsAreSetCorrectly) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableL3FlushAfterPostSync.set(1);
    NEO::debugManager.flags.RedirectFlushL3HostUsmToExternal.set(0);

    auto &productHelper = this->clDevice->getDevice().getProductHelper();
    if (!productHelper.isL3FlushAfterPostSyncSupported(true)) {
        GTEST_SKIP();
    }

    auto commandQueue = createCommandQueue<FamilyType>();
    auto &commandStream = commandQueue->getCS(1024);

    auto &heap = commandQueue->getIndirectHeap(IndirectHeap::Type::dynamicState, 1);
    size_t workSize[] = {1, 1, 1};
    Vec3<size_t> wgInfo = {1, 1, 1};

    size_t commandsOffset = 0;

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(workSize, wgInfo, PreemptionMode::Disabled);
    walkerArgs.blocking = false;

    clDevice->getUltCommandStreamReceiver<FamilyType>().timestampPacketWriteEnabled = true;

    TimestampPacketContainer timestampPacketContainer;
    timestampPacketContainer.add(clDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    walkerArgs.currentTimestampPacketNodes = &timestampPacketContainer;

    mockKernel->kernelInfo.kernelDescriptor.kernelAttributes.flags.usesPrintf = true;
    commandsOffset = commandStream.getUsed();
    HardwareInterface<FamilyType>::template programWalker<WalkerType>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                      heap, heap, heap, dispatchInfo, walkerArgs);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
    auto itorWalker = find<WalkerType *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    EXPECT_NE(hwParse.cmdList.end(), itorWalker);
    auto walkerCmdParsed = genCmdCast<WalkerType *>(*itorWalker);
    EXPECT_NE(nullptr, walkerCmdParsed);

    auto &postSync = walkerCmdParsed->getPostSync();
    EXPECT_TRUE(postSync.getL2TransientFlush());
    EXPECT_FALSE(postSync.getL2Flush());
}
