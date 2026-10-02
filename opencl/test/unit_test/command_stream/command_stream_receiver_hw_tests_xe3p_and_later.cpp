/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

using CommandStreamReceiverTestsXe3pAndLater = UltCommandStreamReceiverTest;

HWTEST2_F(CommandStreamReceiverTestsXe3pAndLater, givenProfilingEnabledWhenBlitBufferThenCommandBufferIsConstructedProperly, IsAtLeastXe3pCore) {
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
        verifyLri(cmdIterator, RegisterOffsets::queueTimestampRegAddressOffsetHigh, timestampContextStartGpuAddress + sizeof(uint32_t));

        verifyLri(++cmdIterator, RegisterOffsets::globalTimestampUn, timestampGlobalStartAddress + sizeof(uint32_t));

        verifyLri(++cmdIterator, RegisterOffsets::queueTimestampRegAddressOffsetLow, timestampContextStartGpuAddress);

        verifyLri(++cmdIterator, RegisterOffsets::globalTimestampLdw, timestampGlobalStartAddress);
    }

    cmdIterator = find<typename FamilyType::XY_COPY_BLT *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);

    cmdIterator = find<typename FamilyType::MI_FLUSH_DW *>(++cmdIterator, cmdList.end());
    ASSERT_NE(cmdList.end(), cmdIterator);

    {

        verifyLri(++cmdIterator, RegisterOffsets::queueTimestampRegAddressOffsetHigh, timestampContextEndGpuAddress + sizeof(uint32_t));

        verifyLri(++cmdIterator, RegisterOffsets::globalTimestampUn, timestampGlobalEndAddress + sizeof(uint32_t));

        verifyLri(++cmdIterator, RegisterOffsets::queueTimestampRegAddressOffsetLow, timestampContextEndGpuAddress);

        verifyLri(++cmdIterator, RegisterOffsets::globalTimestampLdw, timestampGlobalEndAddress);
    }
}

struct SystemMemoryFenceViaComputeWalkerTestXe3pAndLater : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(1);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(0);

        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

HWTEST2_F(SystemMemoryFenceViaComputeWalkerTestXe3pAndLater, givenSystemMemoryFenceGeneratedAsPostSyncOperationInComputeWalkerWhenProgramEnginePrologueIsCalledThenSystemMemFenceAddressIsProgrammed, IsAtLeastXe3pCore) {
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

struct SystemMemoryFenceViaKernelInstructionTestXe3pAndLater : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.set(0);
        debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.set(0);
        debugManager.flags.ProgramGlobalFenceAsKernelInstructionInEUKernel.set(1);

        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

HWTEST2_F(SystemMemoryFenceViaKernelInstructionTestXe3pAndLater, givenSystemMemoryFenceGeneratedAsKernelInstructionInKernelCodeWhenProgramEnginePrologueIsCalledThenSystemMemFenceAddressIsProgrammed, IsAtLeastXe3pCore) {
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

struct MidThreadCommandStreamReceiverTestXe3pAndLater : public UltCommandStreamReceiverTest {
    void SetUp() override {
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::MidThread));

        UltCommandStreamReceiverTest::SetUp();
    }
    DebugManagerStateRestore restore;
};

HWTEST2_F(MidThreadCommandStreamReceiverTestXe3pAndLater, givenMidThreadPreemptionWhenCreatingPreemptionAllocationThenExpectProperAlignment, IsAtLeastXe3pCore) {
    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename FamilyType::STATE_CONTEXT_DATA_BASE_ADDRESS;
    constexpr size_t expectedMask = STATE_CONTEXT_DATA_BASE_ADDRESS::CONTEXTDATABASEADDRESS::CONTEXTDATABASEADDRESS_ALIGN_SIZE - 1;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    GraphicsAllocation *preemptionAllocation = csr.getPreemptionAllocation();
    ASSERT_NE(nullptr, preemptionAllocation);

    size_t addressValue = reinterpret_cast<size_t>(preemptionAllocation->getUnderlyingBuffer());
    EXPECT_EQ(0u, expectedMask & addressValue);
}

struct BcsTestsXe3pAndLater : public UltCommandStreamReceiverTest {
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

HWTEST2_F(BcsTestsXe3pAndLater, givenCompressibleSrcBuffersWhenAppendBlitCommandsBlockCopyIsCalledThenSetCompressionFormat, IsAtLeastXe3pCore) {
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
        0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc, false);
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
