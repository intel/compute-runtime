/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using CommandStreamReceiverXe2AndLater = UltCommandStreamReceiverTest;
using IsWithinXe2HpgCoreAndXe3pCore = IsWithinGfxCore<IGFX_XE2_HPG_CORE, IGFX_XE3P_CORE>;

HWTEST2_F(CommandStreamReceiverXe2AndLater, GivenPreambleNotSentAndDebugFlagEnabledWhenFlushingTaskThenPipelineSelectIsSent, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PipelinedPipelineSelect.set(true);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    if (commandStreamReceiver.heaplessPrologProgrammed) {
        GTEST_SKIP();
    }
    commandStreamReceiver.isPreambleSent = false;

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_NE(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST2_F(CommandStreamReceiverXe2AndLater, GivenPreambleNotSentAndDebugFlagDisabledWhenFlushingTaskThenPipelineSelectIsNotSent, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.PipelinedPipelineSelect.set(false);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = false;

    flushTask(commandStreamReceiver);
    parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    EXPECT_EQ(nullptr, getCommand<typename FamilyType::PIPELINE_SELECT>());
}

HWTEST2_F(CommandStreamReceiverXe2AndLater, givenProfilingEnabledWhenBlitBufferThenCommandBufferIsConstructedProperly, IsWithinXe2HpgCoreAndXe3Core) {
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

HWTEST2_F(CommandStreamReceiverXe2AndLater, givenLastSystolicPipelineSelectModeWhenFlushTaskIsCalledThenDontReprogramPipelineSelect, IsWithinXe2HpgCoreAndXe3pCore) {
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.isPreambleSent = true;
    flushTaskFlags.pipelineSelectArgs.systolicPipelineSelectMode = true;

    flushTask(commandStreamReceiver);
    EXPECT_FALSE(commandStreamReceiver.lastSystolicPipelineSelectMode);
}
