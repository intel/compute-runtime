/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

using namespace NEO;

struct MemorySynchronizationViaMiSemaphoreWaitTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
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
        DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
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
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);
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
    commandStreamReceiver.flushBcsTask(blitPropertiesContainer, false, false, *pDevice);
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
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
        DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);
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

    HardwareInterface<FamilyType>::dispatchWalker(
        commandQueue,
        multiDispatchInfo,
        CsrDependencies(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestampPacket,
        CL_COMMAND_NDRANGE_KERNEL);

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
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(1);
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
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(-1);
        DebugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(-1);
        DebugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(-1);
        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

HWTEST2_F(SystemMemoryFenceInDefaultConfigurationTest, whenEnqueueKernelIsCalledThenFenceCommandsCanBeGenerated, IsPVC) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    VariableBackup<unsigned short> revisionId(&defaultHwInfo->platform.usRevId);

    unsigned short revisions[] = {0x0, 0x3};
    for (auto revision : revisions) {
        revisionId = revision;
        UltClDeviceFactory ultClDeviceFactory{1, 0};
        auto isPvcXlA0Stepping = (revision == 0x0);
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
        if (isPvcXlA0Stepping) {
            EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
            EXPECT_EQ(hwParser.cmdList.end(), itorMiMemFence);
        } else {
            EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
            ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
            auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
            ASSERT_NE(nullptr, fenceCmd);
            EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE, fenceCmd->getFenceType());
        }
    }
}
