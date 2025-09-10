/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_aub_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/event/event.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include <type_traits>

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

using CommandStreamReceiverXe2HpgCoreTests = UltCommandStreamReceiverTest;

XE2_HPG_CORETEST_F(CommandStreamReceiverXe2HpgCoreTests, givenProfilingEnabledWhenBlitBufferThenCommandBufferIsConstructedProperly) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto bcsOsContext = std::unique_ptr<OsContext>(OsContext::create(nullptr, pDevice->getRootDeviceIndex(), 0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, pDevice->getDeviceBitfield())));
    auto bcsCsr = std::make_unique<UltCommandStreamReceiver<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    bcsCsr->setupContext(*bcsOsContext);
    bcsCsr->initializeTagAllocation();

    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();

    auto timestampPacketAllocator = new MockTagAllocator<TimestampPackets<uint64_t, TimestampPacketConstants::preferredPacketCount>>(0, pDevice->getMemoryManager(), bcsCsr->getPreferredTagPoolSize(), gfxCoreHelper.getTimestampPacketAllocatorAlignment(),
                                                                                                                                     sizeof(TimestampPackets<uint64_t, TimestampPacketConstants::preferredPacketCount>), false, bcsOsContext->getDeviceBitfield());

    bcsCsr->timestampPacketAllocator.reset(timestampPacketAllocator);

    auto context = std::make_unique<MockContext>(pClDevice);

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    void *hostPtr = reinterpret_cast<void *>(0x12340000);
    auto graphicsAllocation = buffer->getGraphicsAllocation(pDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForReadWrite(BlitterConstants::BlitDirection::hostPtrToBuffer,
                                                                          *bcsCsr, graphicsAllocation, nullptr, hostPtr,
                                                                          graphicsAllocation->getGpuAddress(), 0,
                                                                          0, 0, {1, 1, 1}, 0, 0, 0, 0);

    MockTimestampPacketContainer timestamp(*bcsCsr->getTimestampPacketAllocator(), 1u);
    blitProperties.blitSyncProperties.outputTimestampPacket = timestamp.getNode(0);
    blitProperties.blitSyncProperties.syncMode = BlitSyncMode::timestamp;

    auto timestampContextStartGpuAddress = TimestampPacketHelper::getContextStartGpuAddress(*blitProperties.blitSyncProperties.outputTimestampPacket);
    auto timestampGlobalStartAddress = TimestampPacketHelper::getGlobalStartGpuAddress(*blitProperties.blitSyncProperties.outputTimestampPacket);

    auto timestampContextEndGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*blitProperties.blitSyncProperties.outputTimestampPacket);
    auto timestampGlobalEndAddress = TimestampPacketHelper::getGlobalEndGpuAddress(*blitProperties.blitSyncProperties.outputTimestampPacket);

    BlitPropertiesContainer blitPropertiesContainer;
    blitPropertiesContainer.push_back(blitProperties);

    bcsCsr->flushBcsTask(blitPropertiesContainer, false, *pDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(bcsCsr->commandStream);
    auto &cmdList = hwParser.cmdList;

    auto cmdIterator = cmdList.begin();

    auto verifyLri = [&](const GenCmdList::iterator &itBegin, uint32_t expectRegister, uint64_t expectedAddress) {
        cmdIterator = find<MI_STORE_REGISTER_MEM *>(itBegin, cmdList.end());
        ASSERT_NE(cmdList.end(), cmdIterator);

        auto lriCmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*cmdIterator);

        EXPECT_EQ(expectRegister + RegisterOffsets::bcs0Base, lriCmd->getRegisterAddress());
        EXPECT_EQ(expectedAddress, lriCmd->getMemoryAddress());
    };

    {
        verifyLri(cmdIterator, RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, timestampContextStartGpuAddress + sizeof(uint32_t));

        verifyLri(++cmdIterator, RegisterOffsets::globalTimestampUn, timestampGlobalStartAddress + sizeof(uint32_t));

        verifyLri(++cmdIterator, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextStartGpuAddress);

        verifyLri(++cmdIterator, RegisterOffsets::globalTimestampLdw, timestampGlobalStartAddress);
    }

    cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);

    cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);

    {

        verifyLri(++cmdIterator, RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, timestampContextEndGpuAddress + sizeof(uint32_t));

        verifyLri(++cmdIterator, RegisterOffsets::globalTimestampUn, timestampGlobalEndAddress + sizeof(uint32_t));

        verifyLri(++cmdIterator, RegisterOffsets::gpThreadTimeRegAddressOffsetLow, timestampContextEndGpuAddress);

        verifyLri(++cmdIterator, RegisterOffsets::globalTimestampLdw, timestampGlobalEndAddress);
    }
}

using MemorySynchronizationViaMiSemaphoreWaitTestXe2HpgCore = MemorySynchronizationViaMiSemaphoreWaitTest;

XE2_HPG_CORETEST_F(MemorySynchronizationViaMiSemaphoreWaitTestXe2HpgCore, givenMemorySynchronizationViaMiSemaphoreWaitWhenProgramEnginePrologueIsCalledThenNoCommandIsProgrammed) {
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

using SystemMemoryFenceInDisabledConfigurationTestXe2HpgCore = SystemMemoryFenceInDisabledConfigurationTest;

XE2_HPG_CORETEST_F(SystemMemoryFenceInDisabledConfigurationTestXe2HpgCore, givenNoSystemMemoryFenceWhenEnqueueKernelIsCalledThenDontGenerateFenceCommands) {
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

using SystemMemoryFenceViaMiMemFenceTestXe2HpgCore = SystemMemoryFenceViaMiMemFenceTest;

XE2_HPG_CORETEST_F(SystemMemoryFenceViaMiMemFenceTestXe2HpgCore, givenCommadStreamReceiverWhenProgramEnginePrologueIsCalledThenIsEnginePrologueSentIsSetToTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    auto requiredSize = commandStreamReceiver.getCmdSizeForPrologue();
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programEnginePrologue(cmdStream);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);
}

XE2_HPG_CORETEST_F(SystemMemoryFenceViaMiMemFenceTestXe2HpgCore, givenIsEnginePrologueSentIsSetToTrueWhenGetRequiredCmdStreamSizeIsCalledThenSizeForEnginePrologueIsNotIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);
    auto sizeForEnginePrologue = commandStreamReceiver.getCmdSizeForPrologue();

    auto sizeWhenEnginePrologueIsNotSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    commandStreamReceiver.isEnginePrologueSent = true;
    auto sizeWhenEnginePrologueIsSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    EXPECT_EQ(sizeForEnginePrologue, sizeWhenEnginePrologueIsNotSent - sizeWhenEnginePrologueIsSent);
}

XE2_HPG_CORETEST_F(SystemMemoryFenceViaMiMemFenceTestXe2HpgCore, givenSystemMemoryFenceGeneratedAsMiFenceCommandInCommandStreamWhenBlitBufferIsCalledThenSystemMemFenceAddressIsProgrammed) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &cmdStream = commandStreamReceiver.getCS(0);

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    BlitPropertiesContainer blitPropertiesContainer;
    commandStreamReceiver.flushBcsTask(blitPropertiesContainer, false, *pDevice);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);

    if (!pClDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdStream);
        auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);

        auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
        EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());
    }
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

using SystemMemoryFenceViaComputeWalkerTestXe2HpgCore = SystemMemoryFenceViaComputeWalkerTest;

XE2_HPG_CORETEST_F(SystemMemoryFenceViaComputeWalkerTestXe2HpgCore, givenSystemMemoryFenceGeneratedAsPostSyncOperationInComputeWalkerWhenProgramEnginePrologueIsCalledThenSystemMemFenceAddressIsProgrammed) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    auto requiredSize = commandStreamReceiver.getCmdSizeForPrologue();
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programEnginePrologue(cmdStream);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);

    if (!pClDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdStream);
        auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);

        auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
        EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());
    }
}

XE2_HPG_CORETEST_F(SystemMemoryFenceViaComputeWalkerTestXe2HpgCore, givenSystemMemoryFenceGeneratedAsPostSyncOperationInComputeWalkerWhenDispatchWalkerIsCalledThenSystemMemoryFenceRequestInPostSyncDataIsProgrammed) {
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

using SystemMemoryFenceViaKernelInstructionTestXe2HpgCore = SystemMemoryFenceViaKernelInstructionTest;

XE2_HPG_CORETEST_F(SystemMemoryFenceViaKernelInstructionTestXe2HpgCore, givenSystemMemoryFenceGeneratedAsKernelInstructionInKernelCodeWhenProgramEnginePrologueIsCalledThenSystemMemFenceAddressIsProgrammed) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);

    auto requiredSize = commandStreamReceiver.getCmdSizeForPrologue();
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programEnginePrologue(cmdStream);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);

    if (!pDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdStream);
        auto itorSystemMemFenceAddress = find<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), itorSystemMemFenceAddress);

        auto systemMemFenceAddressCmd = genCmdCast<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(*itorSystemMemFenceAddress);
        EXPECT_EQ(commandStreamReceiver.globalFenceAllocation->getGpuAddress(), systemMemFenceAddressCmd->getSystemMemoryFenceAddress());
    }
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

using SystemMemoryFenceInDefaultConfigurationTestXe2HpgCore = SystemMemoryFenceInDefaultConfigurationTest;

XE2_HPG_CORETEST_F(SystemMemoryFenceInDefaultConfigurationTestXe2HpgCore,
                   givenNoEventProvidedWhenEnqueueKernelNotUsingSystemMemoryThenNoPostSyncFenceRequestDispatched) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    if (pClDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        GTEST_SKIP();
    }

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, pClDevice, nullptr);
    auto &commandStreamReceiver = pClDevice->getUltCommandStreamReceiver<FamilyType>();

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
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    if (MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pClDevice->getRootDeviceEnvironment()) > 0) {
        auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
        auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
        ASSERT_NE(nullptr, fenceCmd);
        EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
    }
}

XE2_HPG_CORETEST_F(SystemMemoryFenceInDefaultConfigurationTestXe2HpgCore,
                   givenNoEventProvidedWhenEnqueueKernelUsingSystemMemoryThenPostSyncFenceRequestNotDispatched) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    if (pClDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        GTEST_SKIP();
    }

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, pClDevice, nullptr);
    auto &commandStreamReceiver = pClDevice->getUltCommandStreamReceiver<FamilyType>();

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
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    if (MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pClDevice->getRootDeviceEnvironment()) > 0) {
        auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
        auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
        ASSERT_NE(nullptr, fenceCmd);
        EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
    }
}

XE2_HPG_CORETEST_F(SystemMemoryFenceInDefaultConfigurationTestXe2HpgCore,
                   givenEventProvidedWhenEnqueueKernelNotUsingSystemMemoryThenPostSyncFenceRequestNotDispatched) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;
    if (pClDevice->getHardwareInfo().capabilityTable.isIntegratedDevice) {
        GTEST_SKIP();
    }

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueueHw<FamilyType> commandQueue(&context, pClDevice, nullptr);
    auto &commandStreamReceiver = pClDevice->getUltCommandStreamReceiver<FamilyType>();

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
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    if (MemorySynchronizationCommands<FamilyType>::getSizeForAdditionalSynchronization(NEO::FenceType::release, pClDevice->getRootDeviceEnvironment()) > 0) {
        auto itorMiMemFence = find<MI_MEM_FENCE *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), itorMiMemFence);
        auto fenceCmd = genCmdCast<MI_MEM_FENCE *>(*itorMiMemFence);
        ASSERT_NE(nullptr, fenceCmd);
        EXPECT_EQ(MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE, fenceCmd->getFenceType());
    }

    auto event = castToObject<Event>(kernelEvent);
    event->release();
}

struct Xe2MidThreadCommandStreamReceiverTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

XE2_HPG_CORETEST_F(Xe2MidThreadCommandStreamReceiverTest, givenMidThreadPreemptionWhenCreatingPreemptionAllocationThenExpectProperAlignment) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;
    constexpr size_t expectedMask = STATE_CONTEXT_DATA_BASE_ADDRESS::CONTEXTDATABASEADDRESS::CONTEXTDATABASEADDRESS_ALIGN_SIZE - 1;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    GraphicsAllocation *preemptionAllocation = csr.getPreemptionAllocation();
    ASSERT_NE(nullptr, preemptionAllocation);

    size_t addressValue = reinterpret_cast<size_t>(preemptionAllocation->getUnderlyingBuffer());
    EXPECT_EQ(0u, expectedMask & addressValue);
}

using Xe2CommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
XE2_HPG_CORETEST_F(Xe2CommandStreamReceiverFlushTaskTests, givenOverrideThreadArbitrationPolicyDebugVariableSetWhenFlushingThenRequestRequiredMode) {
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    debugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);

    EXPECT_EQ(-1, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);

    flushTask(commandStreamReceiver);
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin,
              commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

XE2_HPG_CORETEST_F(Xe2CommandStreamReceiverFlushTaskTests, givenNotExistPolicyWhenFlushingThenDefaultPolicyIsProgrammed) {
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    int32_t notExistPolicy = -2;
    flushTaskFlags.threadArbitrationPolicy = notExistPolicy;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(notExistPolicy, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

XE2_HPG_CORETEST_F(Xe2CommandStreamReceiverFlushTaskTests, givenLastSystolicPipelineSelectModeWhenFlushTaskIsCalledThenDontReprogramPipelineSelect) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    flushTaskFlags.pipelineSelectArgs.systolicPipelineSelectMode = true;

    flushTask(commandStreamReceiver);
    EXPECT_FALSE(commandStreamReceiver.lastSystolicPipelineSelectMode);
}

struct Xe2BcsTests : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(true);
        UltCommandStreamReceiverTest::SetUp();
        context = std::make_unique<MockContext>(pClDevice);
    }

    void TearDown() override {
        context.reset();
        UltCommandStreamReceiverTest::TearDown();
    }

    DebugManagerStateRestore restore;
    std::unique_ptr<MockContext> context;
    cl_int retVal = CL_SUCCESS;
};

XE2_HPG_CORETEST_F(Xe2BcsTests, givenBufferInDeviceMemoryWhenStatelessCompressionIsEnabledThenBlitterForBufferUsesStatelessCompressedSettings) {
    using MEM_COPY = typename Xe2HpgCoreFamily::MEM_COPY;

    debugManager.flags.BcsCompressionFormatForXe2Plus.set(0x1);

    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(!MemoryPoolHelper::isSystemMemoryPool(allocation->getMemoryPool()));

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocation, 0,
        allocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = Xe2HpgCoreFamily::cmdInitXyCopyBlt;

    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);

    BlitCommandsHelper<Xe2HpgCoreFamily>::appendBlitCommandsForBuffer<MEM_COPY>(blitProperties, *bltCmd, context->getDevice(0)->getRootDeviceEnvironment());

    EXPECT_EQ(static_cast<uint32_t>(debugManager.flags.BcsCompressionFormatForXe2Plus.get()), bltCmd->getCompressionFormat());
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenDstBufferInDeviceAndSrcInSystemMemoryWhenStatelessCompressionIsEnabledThenBlitterForBufferUsesStatelessCompressedSettings) {
    using MEM_COPY = typename Xe2HpgCoreFamily::MEM_COPY;

    debugManager.flags.BcsCompressionFormatForXe2Plus.set(0x1);

    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto bufferDst = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    auto bufferSrc = clUniquePtr<Buffer>(Buffer::create(context.get(), CL_MEM_FORCE_HOST_MEMORY_INTEL, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocationDst = bufferDst->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    auto allocationSrc = bufferSrc->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_FALSE(MemoryPoolHelper::isSystemMemoryPool(allocationDst->getMemoryPool()));
    EXPECT_TRUE(MemoryPoolHelper::isSystemMemoryPool(allocationSrc->getMemoryPool()));

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocationDst, 0,
        allocationSrc, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = Xe2HpgCoreFamily::cmdInitXyCopyBlt;

    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);

    BlitCommandsHelper<Xe2HpgCoreFamily>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, context->getDevice(0)->getRootDeviceEnvironment());

    EXPECT_EQ(static_cast<uint32_t>(debugManager.flags.BcsCompressionFormatForXe2Plus.get()), bltCmd->getCompressionFormat());
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenCompressibleDstBuffersWhenAppendBlitCommandsForBufferCalledThenSetCompressionFormat) {
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    using MEM_COPY = typename FamilyType::MEM_COPY;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto srcBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto dstBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto srcAllocation = srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    srcAllocation->getDefaultGmm()->setCompressionEnabled(false);

    auto dstAllocation = dstBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(dstAllocation->getDefaultGmm()->isCompressionEnabled());

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstAllocation, 0,
        srcAllocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = FamilyType::cmdInitXyCopyBlt;

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, rootDeviceEnvironment);

    auto compressionFormat = 2;

    EXPECT_EQ(compressionFormat, bltCmd->getCompressionFormat());
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenCompressibleSrcBuffersWhenAppendBlitCommandsForBufferCalledThenSetCompressionFormat) {
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    using MEM_COPY = typename FamilyType::MEM_COPY;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto srcBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto dstBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto srcAllocation = srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(srcAllocation->getDefaultGmm()->isCompressionEnabled());

    auto dstAllocation = dstBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    dstAllocation->getDefaultGmm()->setCompressionEnabled(false);

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstAllocation, 0,
        srcAllocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<MEM_COPY>();
    *bltCmd = FamilyType::cmdInitXyCopyBlt;

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, rootDeviceEnvironment);

    auto compressionFormat = 2;

    EXPECT_EQ(compressionFormat, bltCmd->getCompressionFormat());
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenCompressibleSrcBuffersWhenAppendBlitCommandsBlockCopyIsCalledThenSetCompressionFormat) {
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto srcBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto dstBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto srcAllocation = srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(srcAllocation->getDefaultGmm()->isCompressionEnabled());

    auto dstAllocation = dstBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    dstAllocation->getDefaultGmm()->setCompressionEnabled(false);

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstAllocation, 0,
        srcAllocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<XY_BLOCK_COPY_BLT>();
    *bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd->setDestinationX2CoordinateRight(1);
    bltCmd->setDestinationY2CoordinateBottom(1);

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(blitProperties, *bltCmd, rootDeviceEnvironment);

    auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);

    EXPECT_EQ(compressionFormat, bltCmd->getSourceCompressionFormat());
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenCompressibleDstBuffersWhenAppendBlitCommandsBlockCopyIsCalledThenSetCompressionFormat) {
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto srcBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto dstBuffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto srcAllocation = srcBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    srcAllocation->getDefaultGmm()->setCompressionEnabled(false);

    auto dstAllocation = dstBuffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(dstAllocation->getDefaultGmm()->isCompressionEnabled());

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        dstAllocation, 0,
        srcAllocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<XY_BLOCK_COPY_BLT>();
    *bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd->setDestinationX2CoordinateRight(1);
    bltCmd->setDestinationY2CoordinateBottom(1);

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(blitProperties, *bltCmd, rootDeviceEnvironment);

    auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);

    EXPECT_EQ(compressionFormat, bltCmd->getDestinationCompressionFormat());
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenCompressibleBuffersWhenBufferCompressionFormatIsForcedThenCompressionFormatIsSet) {
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    EXPECT_TRUE(allocation->getDefaultGmm()->isCompressionEnabled());

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocation, 0,
        allocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<XY_BLOCK_COPY_BLT>();
    *bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd->setDestinationX2CoordinateRight(1);
    bltCmd->setDestinationY2CoordinateBottom(1);

    uint32_t forcedCompressionFormat = 1;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(forcedCompressionFormat));

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(blitProperties, *bltCmd, rootDeviceEnvironment);

    EXPECT_EQ(forcedCompressionFormat, bltCmd->getDestinationCompressionFormat());
    EXPECT_EQ(forcedCompressionFormat, bltCmd->getSourceCompressionFormat());
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenNotCompressibleBuffersWhenBufferCompressionFormatIsForcedThenCompressionFormatIsNotSet) {
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    allocation->getDefaultGmm()->setCompressionEnabled(false);

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocation, 0,
        allocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<XY_BLOCK_COPY_BLT>();
    *bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd->setDestinationX2CoordinateRight(1);
    bltCmd->setDestinationY2CoordinateBottom(1);

    uint32_t forcedCompressionFormat = 1;
    debugManager.flags.ForceBufferCompressionFormat.set(static_cast<int32_t>(forcedCompressionFormat));

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(blitProperties, *bltCmd, rootDeviceEnvironment);

    EXPECT_EQ(0u, bltCmd->getDestinationCompressionFormat());
    EXPECT_EQ(0u, bltCmd->getSourceCompressionFormat());
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenOverriddenBlitterTargetToZeroWhenAppendBlitCommandsBlockCopyThenUseSystemMem) {
    debugManager.flags.OverrideBlitterTargetMemory.set(0);

    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocation, 0,
        allocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<XY_BLOCK_COPY_BLT>();
    *bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd->setDestinationX2CoordinateRight(1);
    bltCmd->setDestinationY2CoordinateBottom(1);

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(blitProperties, *bltCmd, rootDeviceEnvironment);

    EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
    EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_SYSTEM_MEM);
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenOverriddenBlitterTargetToOneWhenAppendBlitCommandsBlockCopyThenUseLocalMem) {
    debugManager.flags.OverrideBlitterTargetMemory.set(1);

    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocation, 0,
        allocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<XY_BLOCK_COPY_BLT>();
    *bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd->setDestinationX2CoordinateRight(1);
    bltCmd->setDestinationY2CoordinateBottom(1);

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(blitProperties, *bltCmd, rootDeviceEnvironment);

    EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenOverriddenBlitterTargetToTwoWhenAppendBlitCommandsBlockCopyThenUseDefaultMem) {
    debugManager.flags.OverrideBlitterTargetMemory.set(2);

    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocation, 0,
        allocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<XY_BLOCK_COPY_BLT>();
    *bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd->setDestinationX2CoordinateRight(1);
    bltCmd->setDestinationY2CoordinateBottom(1);

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(blitProperties, *bltCmd, rootDeviceEnvironment);

    EXPECT_EQ(bltCmd->getDestinationTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
    EXPECT_EQ(bltCmd->getSourceTargetMemory(), XY_BLOCK_COPY_BLT::TARGET_MEMORY::TARGET_MEMORY_LOCAL_MEM);
}

XE2_HPG_CORETEST_F(Xe2BcsTests, givenOverriddenMocksValueWhenAppendBlitCommandsBlockCopyThenDebugMocksValueIsSet) {
    using XY_BLOCK_COPY_BLT = typename FamilyType::XY_BLOCK_COPY_BLT;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    MockGraphicsAllocation clearColorAlloc;

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, MemoryConstants::pageSize64k, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto allocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());

    auto blitProperties = BlitProperties::constructPropertiesForCopy(
        allocation, 0,
        allocation, 0,
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
    auto bltCmd = stream.getSpaceForCmd<XY_BLOCK_COPY_BLT>();
    *bltCmd = FamilyType::cmdInitXyBlockCopyBlt;
    bltCmd->setDestinationX2CoordinateRight(1);
    bltCmd->setDestinationY2CoordinateBottom(1);

    uint32_t mockValue = context->getDevice(0)->getGmmHelper()->getL3EnabledMOCS();
    uint32_t newValue = mockValue + 1;
    debugManager.flags.OverrideBlitterMocs.set(newValue);

    const auto &rootDeviceEnvironment = context->getDevice(0)->getRootDeviceEnvironment();
    BlitCommandsHelper<FamilyType>::appendBlitCommandsBlockCopy(blitProperties, *bltCmd, rootDeviceEnvironment);

    EXPECT_EQ(bltCmd->getDestinationMOCS(), newValue);
    EXPECT_EQ(bltCmd->getSourceMOCS(), newValue);
}
