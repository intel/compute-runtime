/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

using namespace NEO;

struct MemorySynchronizationViaMiSemaphoreWaitTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
        debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

XE_HPC_CORETEST_F(MemorySynchronizationViaMiSemaphoreWaitTest, givenMemorySynchronizationViaMiSemaphoreWaitWhenProgramEnginePrologueIsCalledThenNoCommandIsProgrammed) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    auto requiredSize = commandStreamReceiver.getCmdSizeForPrologue();
    EXPECT_EQ(0u, requiredSize);

    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programEnginePrologue(cmdStream);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    EXPECT_EQ(0u, hwParser.cmdList.size());
}

struct SystemMemoryFenceInDisabledConfigurationTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
        debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.set(0);
        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

XE_HPC_CORETEST_F(SystemMemoryFenceInDisabledConfigurationTest, givenNoSystemMemoryFenceWhenEnqueueKernelIsCalledThenDontGenerateFenceCommands) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, pClDevice, nullptr);

    size_t globalWorkSize[3] = {1, 1, 1};
    commandQueue.enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandQueue);

    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(hwParser.cmdList.end(), itorSystemMemFenceAddress);

    auto itorComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorComputeWalker);
    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorComputeWalker);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(hwParser.cmdList.end(), itorMiMemFence);
}

struct SystemMemoryFenceViaMiMemFenceTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

XE_HPC_CORETEST_F(SystemMemoryFenceViaMiMemFenceTest, givenCommadStreamReceiverWhenProgramEnginePrologueIsCalledThenIsEnginePrologueSentIsSetToTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    auto requiredSize = commandStreamReceiver.getCmdSizeForPrologue();
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programEnginePrologue(cmdStream);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);
}

XE_HPC_CORETEST_F(SystemMemoryFenceViaMiMemFenceTest, givenIsEnginePrologueSentIsSetToTrueWhenGetRequiredCmdStreamSizeIsCalledThenSizeForEnginePrologueIsNotIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);
    auto sizeForEnginePrologue = commandStreamReceiver.getCmdSizeForPrologue();

    auto sizeWhenEnginePrologueIsNotSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    commandStreamReceiver.isEnginePrologueSent = true;
    auto sizeWhenEnginePrologueIsSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    EXPECT_EQ(sizeForEnginePrologue, sizeWhenEnginePrologueIsNotSent - sizeWhenEnginePrologueIsSent);
}

XE_HPC_CORETEST_F(SystemMemoryFenceViaMiMemFenceTest, givenSystemMemoryFenceGeneratedAsMiFenceCommandInCommandStreamWhenBlitBufferIsCalledThenSystemMemFenceAddressIsProgrammed) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &cmdStream = commandStreamReceiver.getCS(0);

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    BlitPropertiesContainer blitPropertiesContainer;
    commandStreamReceiver.flushBcsTask(blitPropertiesContainer, false, *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);

    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());
}

struct SystemMemoryFenceViaComputeWalkerTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

XE_HPC_CORETEST_F(SystemMemoryFenceViaComputeWalkerTest, givenSystemMemoryFenceGeneratedAsPostSyncOperationInComputeWalkerWhenProgramEnginePrologueIsCalledThenSystemMemFenceAddressIsProgrammed) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    auto requiredSize = commandStreamReceiver.getCmdSizeForPrologue();
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programEnginePrologue(cmdStream);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);

    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());
}

XE_HPC_CORETEST_F(SystemMemoryFenceViaComputeWalkerTest, givenSystemMemoryFenceGeneratedAsPostSyncOperationInComputeWalkerWhenDispatchWalkerIsCalledThenSystemMemoryFenceRequestInPostSyncDataIsProgrammed) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    MockKernelWithInternals kernel(*pClDevice);
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel.mockKernel);
    MockContext context(pClDevice);
    MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
    auto &cmdStream = commandQueue.getCS(0);
    MockTimestampPacketContainer timestampPacket(*pClDevice->getGpgpuCommandStreamReceiver().getTimestampPacketAllocator(), 1);

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    walkerArgs.currentTimestampPacketNodes = &timestampPacket;
    HardwareInterface<FamilyType>::template dispatchWalker<COMPUTE_WALKER>(
        commandQueue,
        multiDispatchInfo,
        CsrDependencies(),
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto itorComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorComputeWalker);

    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorComputeWalker);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
}

struct SystemMemoryFenceViaKernelInstructionTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(1);
        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

XE_HPC_CORETEST_F(SystemMemoryFenceViaKernelInstructionTest, givenSystemMemoryFenceGeneratedAsKernelInstructionInKernelCodeWhenProgramEnginePrologueIsCalledThenSystemMemFenceAddressIsProgrammed) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    auto requiredSize = commandStreamReceiver.getCmdSizeForPrologue();
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programEnginePrologue(cmdStream);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);
    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);

    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());
}

struct SystemMemoryFenceInDefaultConfigurationTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(-1);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(-1);
        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

HWTEST2_F(SystemMemoryFenceInDefaultConfigurationTest, givenSpecificDeviceSteppingWhenEnqueueKernelIsCalledThenFenceCommandsNotGenerated, IsPVC) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    VariableBackup<unsigned short> revisionId(&defaultHwInfo->platform.usRevId);

    constexpr unsigned short revision = 0x0;
    revisionId = revision;
    UltClDeviceFactory ultClDeviceFactory{1, 0};
    auto &clDevice = *ultClDeviceFactory.rootDevices[0];

    MockKernelWithInternals kernel(clDevice);
    MockContext context(&clDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, &clDevice, nullptr);
    auto &commandStreamReceiver = clDevice.getUltCommandStreamReceiver<FamilyType>();

    size_t globalWorkSize[3] = {1, 1, 1};
    commandQueue.enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandQueue);

    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);
    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());

    auto itorComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorComputeWalker);
    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorComputeWalker);
    auto &postSyncData = walkerCmd->getPostSync();

    auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
    EXPECT_EQ(hwParser.cmdList.end(), itorMiMemFence);
}

XE_HPC_CORETEST_F(SystemMemoryFenceInDefaultConfigurationTest,
                  givenNoEventProvidedWhenEnqueueKernelNotUsingSystemMemoryThenNoPostSyncFenceRequestDispatched) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    VariableBackup<unsigned short> revisionId(&defaultHwInfo->platform.usRevId);

    constexpr unsigned short revision = 0x3;
    revisionId = revision;
    UltClDeviceFactory ultClDeviceFactory{1, 0};
    auto &clDevice = *ultClDeviceFactory.rootDevices[0];

    MockKernelWithInternals kernel(clDevice);
    MockContext context(&clDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, &clDevice, nullptr);
    auto &commandStreamReceiver = clDevice.getUltCommandStreamReceiver<FamilyType>();

    size_t globalWorkSize[3] = {1, 1, 1};
    commandQueue.enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandQueue);

    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);
    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());

    auto itorComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorComputeWalker);
    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorComputeWalker);
    auto &postSyncData = walkerCmd->getPostSync();

    auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
    ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
    auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
    ASSERT_NE(nullptr, fenceCmd);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
}

XE_HPC_CORETEST_F(SystemMemoryFenceInDefaultConfigurationTest,
                  givenNoEventProvidedWhenEnqueueKernelUsingSystemMemoryThenPostSyncFenceRequestNotDispatched) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    VariableBackup<unsigned short> revisionId(&defaultHwInfo->platform.usRevId);

    constexpr unsigned short revision = 0x3;
    revisionId = revision;
    UltClDeviceFactory ultClDeviceFactory{1, 0};
    auto &clDevice = *ultClDeviceFactory.rootDevices[0];

    MockKernelWithInternals kernel(clDevice);
    MockContext context(&clDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, &clDevice, nullptr);
    auto &commandStreamReceiver = clDevice.getUltCommandStreamReceiver<FamilyType>();

    size_t globalWorkSize[3] = {1, 1, 1};
    kernel.mockKernel->anyKernelArgumentUsingSystemMemory = true;
    commandQueue.enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, nullptr);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandQueue);

    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);
    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());

    auto itorComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorComputeWalker);
    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorComputeWalker);
    auto &postSyncData = walkerCmd->getPostSync();

    auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
    ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
    auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
    ASSERT_NE(nullptr, fenceCmd);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
}

XE_HPC_CORETEST_F(SystemMemoryFenceInDefaultConfigurationTest,
                  givenEventProvidedWhenEnqueueKernelNotUsingSystemMemoryThenPostSyncFenceRequestNotDispatched) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    VariableBackup<unsigned short> revisionId(&defaultHwInfo->platform.usRevId);

    constexpr unsigned short revision = 0x3;
    revisionId = revision;
    UltClDeviceFactory ultClDeviceFactory{1, 0};
    auto &clDevice = *ultClDeviceFactory.rootDevices[0];

    MockKernelWithInternals kernel(clDevice);
    MockContext context(&clDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, &clDevice, nullptr);
    auto &commandStreamReceiver = clDevice.getUltCommandStreamReceiver<FamilyType>();

    size_t globalWorkSize[3] = {1, 1, 1};
    cl_event kernelEvent{};
    commandQueue.enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, &kernelEvent);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandQueue);

    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);
    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());

    auto itorComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorComputeWalker);
    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorComputeWalker);
    auto &postSyncData = walkerCmd->getPostSync();

    auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
    ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
    auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
    ASSERT_NE(nullptr, fenceCmd);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());

    auto event = castToObject<Event>(kernelEvent);
    event->release();
}

XE_HPC_CORETEST_F(SystemMemoryFenceInDefaultConfigurationTest,
                  givenEventProvidedWhenEnqueueKernelUsingSystemMemoryThenPostSyncFenceRequestDispatched) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    VariableBackup<unsigned short> revisionId(&defaultHwInfo->platform.usRevId);

    constexpr unsigned short revision = 0x3;
    revisionId = revision;
    UltClDeviceFactory ultClDeviceFactory{1, 0};
    auto &clDevice = *ultClDeviceFactory.rootDevices[0];

    MockKernelWithInternals kernel(clDevice);
    MockContext context(&clDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, &clDevice, nullptr);
    auto &commandStreamReceiver = clDevice.getUltCommandStreamReceiver<FamilyType>();

    size_t globalWorkSize[3] = {1, 1, 1};
    cl_event kernelEvent{};
    kernel.mockKernel->anyKernelArgumentUsingSystemMemory = true;
    commandQueue.enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, &kernelEvent);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandQueue);

    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);
    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());

    auto itorComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorComputeWalker);
    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorComputeWalker);
    auto &postSyncData = walkerCmd->getPostSync();

    auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
    ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
    auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
    ASSERT_NE(nullptr, fenceCmd);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());

    auto event = castToObject<Event>(kernelEvent);
    event->release();
}

XE_HPC_CORETEST_F(SystemMemoryFenceInDefaultConfigurationTest,
                  givenEventProvidedWhenEnqueueBuiltinKernelUsingSystemMemoryInDestinationArgumentThenPostSyncFenceRequestDispatched) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    VariableBackup<unsigned short> revisionId(&defaultHwInfo->platform.usRevId);

    constexpr unsigned short revision = 0x3;
    revisionId = revision;
    UltClDeviceFactory ultClDeviceFactory{1, 0};
    auto &clDevice = *ultClDeviceFactory.rootDevices[0];

    MockKernelWithInternals kernel(clDevice);
    MockContext context(&clDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, &clDevice, nullptr);
    auto &commandStreamReceiver = clDevice.getUltCommandStreamReceiver<FamilyType>();

    size_t globalWorkSize[3] = {1, 1, 1};
    cl_event kernelEvent{};
    kernel.mockKernel->isBuiltIn = true;
    kernel.mockKernel->setDestinationAllocationInSystemMemory(true);
    commandQueue.enqueueKernel(kernel, 1, nullptr, globalWorkSize, nullptr, 0, nullptr, &kernelEvent);

    ClHardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(commandQueue);

    auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);
    auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
    EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());

    auto itorComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorComputeWalker);
    auto walkerCmd = genCmdCast<COMPUTE_WALKER *>(*itorComputeWalker);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());

    auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
    auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
    ASSERT_NE(nullptr, fenceCmd);
    EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());

    auto event = castToObject<Event>(kernelEvent);
    event->release();
}
