/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

using namespace NEO;

struct EnqueueFixtureXe2HpgCore : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableMemoryPrefetch.set(1);

        clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), mockRootDeviceIndex));
        context = std::make_unique<MockContext>(clDevice.get());

        mockKernel = std::make_unique<MockKernelWithInternals>(*clDevice, context.get());
        mockKernel->kernelInfo.createKernelAllocation(clDevice->getDevice(), false);

        dispatchInfo = {clDevice.get(), mockKernel->mockKernel, 1, 0, 0, 0};
    }

    void TearDown() override {
        clDevice->getMemoryManager()->freeGraphicsMemory(mockKernel->kernelInfo.getGraphicsAllocation());
    }

    template <typename FamilyType>
    std::unique_ptr<MockCommandQueueHw<FamilyType>> createCommandQueue() {
        return std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), clDevice.get(), nullptr);
    }

    DebugManagerStateRestore restore;
    std::unique_ptr<MockClDevice> clDevice;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockKernelWithInternals> mockKernel;
    DispatchInfo dispatchInfo;
};

using MemoryPrefetchTestsXe2HpgCore = EnqueueFixtureXe2HpgCore;

XE2_HPG_CORETEST_F(MemoryPrefetchTestsXe2HpgCore, givenKernelWhenWalkerIsProgrammedThenPrefetchIsaBeforeWalker) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;

    auto commandQueue = createCommandQueue<FamilyType>();
    auto &commandStream = commandQueue->getCS(1024);

    auto &heap = commandQueue->getIndirectHeap(IndirectHeap::Type::dynamicState, 1);
    size_t workSize[] = {1, 1, 1};
    Vec3<size_t> wgInfo = {1, 1, 1};

    mockKernel->kernelInfo.heapInfo.kernelHeapSize = 1;

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(workSize, wgInfo, PreemptionMode::Disabled);

    HardwareInterface<FamilyType>::template programWalker<COMPUTE_WALKER>(commandStream, *mockKernel->mockKernel, *commandQueue, heap, heap, heap, dispatchInfo, walkerArgs);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(commandStream, 0);
    auto itorWalker = find<COMPUTE_WALKER *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    EXPECT_NE(hwParse.cmdList.end(), itorWalker);

    auto itorStatePrefetch = find<STATE_PREFETCH *>(hwParse.cmdList.begin(), itorWalker);
    EXPECT_NE(itorWalker, itorStatePrefetch);

    auto statePrefetchCmd = genCmdCast<STATE_PREFETCH *>(*itorStatePrefetch);
    EXPECT_NE(nullptr, statePrefetchCmd);

    EXPECT_EQ(mockKernel->kernelInfo.getGraphicsAllocation()->getGpuAddress(), statePrefetchCmd->getAddress());
    EXPECT_TRUE(statePrefetchCmd->getKernelInstructionPrefetch());
}

XE2_HPG_CORETEST_F(MemoryPrefetchTestsXe2HpgCore, givenPrefetchEnabledWhenEstimatingCommandsSizeThenAddStatePrefetch) {
    auto commandQueue = createCommandQueue<FamilyType>();

    size_t numPipeControls = MemorySynchronizationCommands<FamilyType>::isBarrierWaRequired(clDevice->getRootDeviceEnvironment()) ? 2 : 1;

    size_t expected = sizeof(typename FamilyType::COMPUTE_WALKER) +
                      (sizeof(typename FamilyType::PIPE_CONTROL) * numPipeControls) +
                      HardwareCommandsHelper<FamilyType>::getSizeRequiredCS() +
                      EncodeMemoryPrefetch<FamilyType>::getSizeForMemoryPrefetch(mockKernel->kernelInfo.heapInfo.kernelHeapSize, clDevice->getRootDeviceEnvironment());

    EXPECT_EQ(expected, EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *commandQueue, mockKernel->mockKernel, {}));
}

using ProgramWalkerTestsXe2HpgCore = EnqueueFixtureXe2HpgCore;

XE2_HPG_CORETEST_F(ProgramWalkerTestsXe2HpgCore, givenDebugVariableSetWhenProgrammingWalkerThenSetL3Prefetch) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    auto commandQueue = createCommandQueue<FamilyType>();
    auto &commandStream = commandQueue->getCS(1024);

    auto &heap = commandQueue->getIndirectHeap(IndirectHeap::Type::dynamicState, 1);
    size_t workSize[] = {1, 1, 1};
    Vec3<size_t> wgInfo = {1, 1, 1};

    size_t commandsOffset = 0;

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(workSize, wgInfo, PreemptionMode::Disabled);

    {
        // default
        HardwareInterface<FamilyType>::template programWalker<COMPUTE_WALKER>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                              heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, 0);
        auto itorWalker = find<COMPUTE_WALKER *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_FALSE(walkerCmd->getL3PrefetchDisable());
    }

    {
        // debug flag == 1

        commandsOffset = commandStream.getUsed();
        debugManager.flags.ForceL3PrefetchForComputeWalker.set(1);

        HardwareInterface<FamilyType>::template programWalker<COMPUTE_WALKER>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                              heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
        auto itorWalker = find<COMPUTE_WALKER *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_FALSE(walkerCmd->getL3PrefetchDisable());
    }

    {
        // debug flag == 0

        commandsOffset = commandStream.getUsed();
        debugManager.flags.ForceL3PrefetchForComputeWalker.set(0);

        HardwareInterface<FamilyType>::template programWalker<COMPUTE_WALKER>(commandStream, *mockKernel->mockKernel, *commandQueue,
                                                                              heap, heap, heap, dispatchInfo, walkerArgs);

        HardwareParse hwParse;
        hwParse.parseCommands<FamilyType>(commandStream, commandsOffset);
        auto itorWalker = find<COMPUTE_WALKER *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
        EXPECT_NE(hwParse.cmdList.end(), itorWalker);
        auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorWalker);
        EXPECT_NE(nullptr, walkerCmd);

        EXPECT_TRUE(walkerCmd->getL3PrefetchDisable());
    }
}
