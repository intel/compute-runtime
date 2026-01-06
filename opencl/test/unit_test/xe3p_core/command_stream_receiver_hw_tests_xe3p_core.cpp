/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/append_operations.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/extensions/public/cl_ext_private.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/command_queue/hardware_interface_helper.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"

#include "gtest/gtest.h"
#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

#include <cstdint>
#include <memory>

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

using CommandStreamReceiverXe3pCoreTests = UltCommandStreamReceiverTest;

XE3P_CORETEST_F(CommandStreamReceiverXe3pCoreTests, givenProfilingEnabledWhenBlitBufferThenCommandBufferIsConstructedProperly) {
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

using MemorySynchronizationViaMiSemaphoreWaitTestXe3pCore = MemorySynchronizationViaMiSemaphoreWaitTest;

XE3P_CORETEST_F(MemorySynchronizationViaMiSemaphoreWaitTestXe3pCore, givenMemorySynchronizationViaMiSemaphoreWaitWhenProgramEnginePrologueIsCalledThenNoCommandIsProgrammed) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    if (commandStreamReceiver.heaplessStateInitialized) {
        EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);
        commandStreamReceiver.isEnginePrologueSent = false;
    } else {
        EXPECT_FALSE(commandStreamReceiver.isEnginePrologueSent);
    }

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

struct SystemMemoryFenceViaMiMemFenceTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(1);

        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

using SystemMemoryFenceViaMiMemFenceTestXe3pCore = SystemMemoryFenceViaMiMemFenceTest;

XE3P_CORETEST_F(SystemMemoryFenceViaMiMemFenceTestXe3pCore, givenCommadStreamReceiverWhenProgramEnginePrologueIsCalledThenIsEnginePrologueSentIsSetToTrue) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    EXPECT_EQ(commandStreamReceiver.heaplessStateInitialized, commandStreamReceiver.isEnginePrologueSent);
    commandStreamReceiver.isEnginePrologueSent = false;
    auto requiredSize = commandStreamReceiver.getCmdSizeForPrologue();
    StackVec<char, 4096> buffer(requiredSize);
    LinearStream cmdStream(buffer.begin(), buffer.size());

    commandStreamReceiver.programEnginePrologue(cmdStream);
    EXPECT_TRUE(commandStreamReceiver.isEnginePrologueSent);
}

XE3P_CORETEST_F(SystemMemoryFenceViaMiMemFenceTestXe3pCore, givenIsEnginePrologueSentIsSetToTrueWhenGetRequiredCmdStreamSizeIsCalledThenSizeForEnginePrologueIsNotIncluded) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    auto dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    EXPECT_EQ(commandStreamReceiver.heaplessStateInitialized, commandStreamReceiver.isEnginePrologueSent);
    commandStreamReceiver.isEnginePrologueSent = false;

    auto sizeForEnginePrologue = commandStreamReceiver.getCmdSizeForPrologue();

    auto sizeWhenEnginePrologueIsNotSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    commandStreamReceiver.isEnginePrologueSent = true;
    auto sizeWhenEnginePrologueIsSent = commandStreamReceiver.getRequiredCmdStreamSize(dispatchFlags, *pDevice);

    EXPECT_EQ(sizeForEnginePrologue, sizeWhenEnginePrologueIsNotSent - sizeWhenEnginePrologueIsSent);
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

using SystemMemoryFenceViaComputeWalkerTestXe3pCore = SystemMemoryFenceViaComputeWalkerTest;

XE3P_CORETEST_F(SystemMemoryFenceViaComputeWalkerTestXe3pCore, givenSystemMemoryFenceGeneratedAsPostSyncOperationInComputeWalkerWhenProgramEnginePrologueIsCalledThenSystemMemFenceAddressIsProgrammed) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    commandStreamReceiver.isEnginePrologueSent = false;

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

using SystemMemoryFenceViaKernelInstructionTestXe3pCore = SystemMemoryFenceViaKernelInstructionTest;

XE3P_CORETEST_F(SystemMemoryFenceViaKernelInstructionTestXe3pCore, givenSystemMemoryFenceGeneratedAsKernelInstructionInKernelCodeWhenProgramEnginePrologueIsCalledThenSystemMemFenceAddressIsProgrammed) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isEnginePrologueSent = false;

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

struct Xe3pMidThreadCommandStreamReceiverTest : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

XE3P_CORETEST_F(Xe3pMidThreadCommandStreamReceiverTest, givenMidThreadPreemptionWhenCreatingPreemptionAllocationThenExpectProperAlignment) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;
    constexpr size_t expectedMask = STATE_CONTEXT_DATA_BASE_ADDRESS::CONTEXTDATABASEADDRESS::CONTEXTDATABASEADDRESS_ALIGN_SIZE - 1;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    GraphicsAllocation *preemptionAllocation = csr.getPreemptionAllocation();
    ASSERT_NE(nullptr, preemptionAllocation);

    size_t addressValue = reinterpret_cast<size_t>(preemptionAllocation->getUnderlyingBuffer());
    EXPECT_EQ(0u, expectedMask & addressValue);
}

using Xe3pCommandStreamReceiverFlushTaskTests = UltCommandStreamReceiverTest;
XE3P_CORETEST_F(Xe3pCommandStreamReceiverFlushTaskTests, givenOverrideThreadArbitrationPolicyDebugVariableSetWhenFlushingThenRequestRequiredMode) {
    DebugManagerStateRestore restore;
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }

    debugManager.flags.OverrideThreadArbitrationPolicy.set(ThreadArbitrationPolicy::RoundRobin);
    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);
    commandStreamReceiver.streamProperties.stateComputeMode.initSupport(pDevice->getRootDeviceEnvironment());

    EXPECT_EQ(-1, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);

    flushTask(commandStreamReceiver);
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin,
              commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

XE3P_CORETEST_F(Xe3pCommandStreamReceiverFlushTaskTests, givenNotExistPolicyWhenFlushingThenDefaultPolicyIsProgrammed) {
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore;
    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);

    commandStreamReceiver.streamProperties.stateComputeMode.initSupport(pDevice->getRootDeviceEnvironment());

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    int32_t notExistPolicy = -2;
    flushTaskFlags.threadArbitrationPolicy = notExistPolicy;

    flushTask(commandStreamReceiver);

    EXPECT_EQ(notExistPolicy, commandStreamReceiver.streamProperties.stateComputeMode.threadArbitrationPolicy.value);
}

XE3P_CORETEST_F(Xe3pCommandStreamReceiverFlushTaskTests, givenLastSystolicPipelineSelectModeWhenFlushTaskIsCalledThenDontReprogramPipelineSelect) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    flushTaskFlags.pipelineSelectArgs.systolicPipelineSelectMode = true;

    flushTask(commandStreamReceiver);
    EXPECT_FALSE(commandStreamReceiver.lastSystolicPipelineSelectMode);
}

struct Xe3pBcsTests : public UltCommandStreamReceiverTest {
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

XE3P_CORETEST_F(Xe3pBcsTests, givenBufferInDeviceMemoryWhenStatelessCompressionIsEnabledThenBlitterForBufferUsesStatelessCompressedSettings) {
    using MEM_COPY = typename Xe3pCoreFamily::MEM_COPY;

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
    *bltCmd = Xe3pCoreFamily::cmdInitXyCopyBlt;

    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);

    BlitCommandsHelper<Xe3pCoreFamily>::appendBlitCommandsForBuffer<MEM_COPY>(blitProperties, *bltCmd, context->getDevice(0)->getRootDeviceEnvironment());

    EXPECT_EQ(static_cast<uint32_t>(debugManager.flags.BcsCompressionFormatForXe2Plus.get()), bltCmd->getCompressionFormat());
}

XE3P_CORETEST_F(Xe3pBcsTests, givenDstBufferInDeviceAndSrcInSystemMemoryWhenStatelessCompressionIsEnabledThenBlitterForBufferUsesStatelessCompressedSettings) {
    using MEM_COPY = typename Xe3pCoreFamily::MEM_COPY;

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
    *bltCmd = Xe3pCoreFamily::cmdInitXyCopyBlt;

    debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.set(true);

    BlitCommandsHelper<Xe3pCoreFamily>::appendBlitCommandsForBuffer(blitProperties, *bltCmd, context->getDevice(0)->getRootDeviceEnvironment());

    EXPECT_EQ(static_cast<uint32_t>(debugManager.flags.BcsCompressionFormatForXe2Plus.get()), bltCmd->getCompressionFormat());
}

XE3P_CORETEST_F(Xe3pBcsTests, givenCompressibleDstBuffersWhenAppendBlitCommandsForBufferCalledThenSetCompressionFormat) {
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

    auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);

    EXPECT_EQ(compressionFormat, bltCmd->getCompressionFormat());
}

XE3P_CORETEST_F(Xe3pBcsTests, givenCompressibleSrcBuffersWhenAppendBlitCommandsForBufferCalledThenSetCompressionFormat) {
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

    auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
    auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);

    EXPECT_EQ(compressionFormat, bltCmd->getCompressionFormat());
}

XE3P_CORETEST_F(Xe3pBcsTests, givenCompressibleSrcBuffersWhenAppendBlitCommandsBlockCopyIsCalledThenSetCompressionFormat) {
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

XE3P_CORETEST_F(Xe3pBcsTests, givenCompressibleDstBuffersWhenAppendBlitCommandsBlockCopyIsCalledThenSetCompressionFormat) {
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

XE3P_CORETEST_F(Xe3pBcsTests, givenCompressibleBuffersWhenBufferCompressionFormatIsForcedThenCompressionFormatIsSet) {
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

XE3P_CORETEST_F(Xe3pBcsTests, givenNotCompressibleBuffersWhenBufferCompressionFormatIsForcedThenCompressionFormatIsNotSet) {
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

XE3P_CORETEST_F(Xe3pBcsTests, givenOverriddenBlitterTargetToZeroWhenAppendBlitCommandsBlockCopyThenUseSystemMem) {
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

XE3P_CORETEST_F(Xe3pBcsTests, givenOverriddenBlitterTargetToOneWhenAppendBlitCommandsBlockCopyThenUseLocalMem) {
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

XE3P_CORETEST_F(Xe3pBcsTests, givenOverriddenBlitterTargetToTwoWhenAppendBlitCommandsBlockCopyThenUseDefaultMem) {
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

XE3P_CORETEST_F(Xe3pBcsTests, givenOverriddenMocksValueWhenAppendBlitCommandsBlockCopyThenMocksValueIsSet) {
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

using CommandStreamReceiverXe3pCoreComputeWalker2Tests = UltCommandStreamReceiverTest;

XE3P_CORETEST_F(CommandStreamReceiverXe3pCoreComputeWalker2Tests, givenHeaplessModeEnabledWhenDispatchKernelThenCorrectCmdsAreProgrammed) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    MockKernelWithInternals kernel(*pClDevice);
    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel.mockKernel);
    MockContext context(pClDevice);
    MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
    commandQueue.heaplessModeEnabled = true;
    auto &cmdStream = commandQueue.getCS(0);

    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);
    HardwareInterface<FamilyType>::template dispatchWalker<DefaultWalkerType>(
        commandQueue,
        multiDispatchInfo,
        CsrDependencies(),
        walkerArgs);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream);

    auto itComputeWalker = find<COMPUTE_WALKER *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(hwParser.cmdList.end(), itComputeWalker);

    auto itComputeWalker2 = find<DefaultWalkerType *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_NE(hwParser.cmdList.end(), itComputeWalker2);
}

XE3P_CORETEST_F(CommandStreamReceiverXe3pCoreComputeWalker2Tests, whenDispatchKernelThenEuThreadSchedulingModeIsCorrect) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;
    using EU_THREAD_SCHEDULING_MODE_OVERRIDE = typename INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE;

    DebugManagerStateRestore restore;
    MockKernelWithInternals kernel(*pClDevice);
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, kernel.mockKernel);
    MockContext context(pClDevice);
    HardwareInterfaceWalkerArgs walkerArgs = createHardwareInterfaceWalkerArgs(CL_COMMAND_NDRANGE_KERNEL);

    {

        NEO::debugManager.flags.OverrideThreadArbitrationPolicy.set(-1);
        MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
        commandQueue.heaplessModeEnabled = true;

        auto &cmdStream = commandQueue.getCS(0);

        HardwareInterface<FamilyType>::template dispatchWalker<WalkerType>(
            commandQueue,
            multiDispatchInfo,
            CsrDependencies(),
            walkerArgs);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdStream);

        auto itComputeWalker2 = find<WalkerType *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_NE(hwParser.cmdList.end(), itComputeWalker2);

        auto &idd2 = reinterpret_cast<WalkerType *>(*itComputeWalker2)->getInterfaceDescriptor();

        auto expectedPipelinedThreadArbitrationPolicy = EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN;

        EXPECT_EQ(expectedPipelinedThreadArbitrationPolicy, static_cast<int32_t>(idd2.getEuThreadSchedulingModeOverride()));
    }
    {
        int32_t forcedRoundRobin = ThreadArbitrationPolicy::RoundRobin;
        int32_t expectedPolicy = EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN;

        NEO::debugManager.flags.OverrideThreadArbitrationPolicy.set(forcedRoundRobin);

        MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
        commandQueue.heaplessModeEnabled = true;

        auto &cmdStream = commandQueue.getCS(0);

        HardwareInterface<FamilyType>::template dispatchWalker<WalkerType>(
            commandQueue,
            multiDispatchInfo,
            CsrDependencies(),
            walkerArgs);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdStream);

        auto itComputeWalker2 = find<WalkerType *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        EXPECT_NE(hwParser.cmdList.end(), itComputeWalker2);

        auto &idd2 = reinterpret_cast<WalkerType *>(*itComputeWalker2)->getInterfaceDescriptor();

        EXPECT_EQ(expectedPolicy, static_cast<int32_t>(idd2.getEuThreadSchedulingModeOverride()));
    }
}

XE3P_CORETEST_F(CommandStreamReceiverXe3pCoreComputeWalker2Tests, GivenComputeWalker2yWhenSendIndirectStateIsCalledThenIohIsCorrectlyAligned) {
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto context = std::make_unique<MockContext>(pClDevice);
    MockCommandQueue commandQueue(context.get(), pClDevice, nullptr, false);

    auto &commandStream = commandQueue.getCS(1024);
    auto walkerCmd = static_cast<WalkerType *>(commandStream.getSpace(sizeof(WalkerType)));
    *walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    auto &idd = walkerCmd->getInterfaceDescriptor();

    auto &dsh = commandQueue.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = commandQueue.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};
    const uint32_t threadGroupCount = 1u;
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType());

    std::unique_ptr<MockKernelWithInternals> mockKernelWithInternal = std::make_unique<MockKernelWithInternals>(*pClDevice, context.get());

    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel);

    auto usedBeforeIOH = ioh.getUsed();

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<WalkerType, INTERFACE_DESCRIPTOR_DATA_2>(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        walkerCmd,
        &idd,
        true,
        0,
        *pDevice);

    auto usedAfterIOH = ioh.getUsed();
    auto iohDiff = usedAfterIOH - usedBeforeIOH;
    auto iohDiffAligned = alignUp(iohDiff, EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment());
    EXPECT_EQ(iohDiffAligned, iohDiff);
}
