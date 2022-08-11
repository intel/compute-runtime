/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "hw_cmds_xe_hpc_core_base.h"

using namespace NEO;

struct BlitXeHpcCoreTests : public ::testing::Test {
    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        DebugManager.flags.RenderCompressedBuffersEnabled.set(true);
        DebugManager.flags.EnableLocalMemory.set(true);
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = true;

        clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    }

    std::optional<uint32_t> flushBcsTask(CommandStreamReceiver *csr, const BlitProperties &blitProperties, bool blocking, Device &device) {
        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        return csr->flushBcsTask(blitPropertiesContainer, blocking, false, device);
    }

    std::unique_ptr<MockClDevice> clDevice;
    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    DebugManagerStateRestore debugRestorer;
};

XE_HPC_CORETEST_F(BlitXeHpcCoreTests, givenCompressedBufferWhenProgrammingBltCommandThenSetCompressionFields) {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular).commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto bufferCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled = true;
    auto bufferNotCompressed = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex())->getDefaultGmm()->isCompressionEnabled = false;
    MockGraphicsAllocation clearColorAlloc;

    {
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto itorBltCmd = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), itorBltCmd);
        MEM_COPY *bltCmd = (MEM_COPY *)*itorBltCmd;

        EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_DISABLE);
        EXPECT_EQ(bltCmd->getDestinationCompressible(), MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_NOT_COMPRESSIBLE);
        EXPECT_EQ(bltCmd->getSourceCompressible(), MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
    }

    {
        auto offset = csr->commandStream.getUsed();
        auto blitProperties = BlitProperties::constructPropertiesForCopy(bufferCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         bufferNotCompressed->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                         0, 0, {2048, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

        flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto bltCmd = genCmdCast<MEM_COPY *>(*(hwParser.cmdList.begin()));
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_ENABLE);
        EXPECT_EQ(bltCmd->getDestinationCompressible(), MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
        EXPECT_EQ(bltCmd->getSourceCompressible(), MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_NOT_COMPRESSIBLE);
    }
}

XE_HPC_CORETEST_F(BlitXeHpcCoreTests, givenBufferWhenProgrammingBltCommandThenSetMocs) {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     0, 0, {1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto itorBltCmd = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorBltCmd);
    MEM_COPY *bltCmd = (MEM_COPY *)*itorBltCmd;

    auto mocsL3enabled = 0x10u;
    EXPECT_EQ(mocsL3enabled, bltCmd->getDestinationMOCS());
    EXPECT_EQ(mocsL3enabled, bltCmd->getSourceMOCS());
}

XE_HPC_CORETEST_F(BlitXeHpcCoreTests, givenTransferLargerThenHalfOfL3WhenItIsProgrammedThenL3IsDisabled) {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    size_t transferSize = static_cast<size_t>(clDevice->getHardwareInfo().gtSystemInfo.L3CacheSizeInKb * KB / 2 + 1);

    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     0, 0, {transferSize, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto itorBltCmd = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorBltCmd);
    MEM_COPY *bltCmd = (MEM_COPY *)*itorBltCmd;

    auto mocsL3disabled = 0x0u;
    EXPECT_EQ(mocsL3disabled, bltCmd->getDestinationMOCS());
    EXPECT_EQ(mocsL3disabled, bltCmd->getSourceMOCS());
}

XE_HPC_CORETEST_F(BlitXeHpcCoreTests, givenBufferWhenProgrammingBltCommandThenSetMocsToValueOfDebugKey) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideBlitterMocs.set(0u);
    using MEM_COPY = typename FamilyType::MEM_COPY;
    MockGraphicsAllocation clearColorAlloc;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()),
                                                                     0, 0, {1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto itorBltCmd = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorBltCmd);
    MEM_COPY *bltCmd = (MEM_COPY *)*itorBltCmd;

    EXPECT_EQ(0u, bltCmd->getDestinationMOCS());
    EXPECT_EQ(0u, bltCmd->getSourceMOCS());
}

XE_HPC_CORETEST_F(BlitXeHpcCoreTests, givenCompressedBufferWhenResolveBlitIsCalledThenProgramSpecialOperationMode) {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 2048, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForAuxTranslation(AuxTranslationDirection::AuxToNonAux,
                                                                               buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()), &clearColorAlloc);

    flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto itorBltCmd = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorBltCmd);
    MEM_COPY *bltCmd = (MEM_COPY *)*itorBltCmd;

    EXPECT_EQ(bltCmd->getDestinationCompressionEnable(), MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    EXPECT_EQ(bltCmd->getDestinationCompressible(), MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
    EXPECT_EQ(bltCmd->getSourceCompressible(), MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
}

XE_HPC_CORETEST_F(BlitXeHpcCoreTests, given2dBlitCommandWhenDispatchingThenSetValidSurfaceType) {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto allocation = buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex());
    MockGraphicsAllocation clearColorAlloc;

    size_t offset = 0;
    {
        // 1D
        auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                         0, 0, {BlitterConstants::maxBlitWidth - 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
        flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream);

        auto cmdIterator = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

        auto bltCmd = genCmdCast<MEM_COPY *>(*cmdIterator);
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(MEM_COPY::COPY_TYPE::COPY_TYPE_LINEAR_COPY, bltCmd->getCopyType());

        offset = csr->commandStream.getUsed();
        MockGraphicsAllocation clearColorAlloc;
    }

    {
        // 2D
        auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, allocation,
                                                                         0, 0, {(2 * BlitterConstants::maxBlitWidth) + 1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);
        flushBcsTask(csr, blitProperties, false, clDevice->getDevice());

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, offset);

        auto cmdIterator = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        ASSERT_NE(hwParser.cmdList.end(), cmdIterator);

        auto bltCmd = genCmdCast<MEM_COPY *>(*cmdIterator);
        EXPECT_NE(nullptr, bltCmd);

        EXPECT_EQ(MEM_COPY::COPY_TYPE::COPY_TYPE_MATRIX_COPY, bltCmd->getCopyType());
    }
}

HWTEST_EXCLUDE_PRODUCT(CommandQueueHwTest, givenCommandQueueWhenAskingForCacheFlushOnBcsThenReturnTrue, IGFX_XE_HPC_CORE);

using XeHpcCoreCopyEngineTests = ::testing::Test;
XE_HPC_CORETEST_F(XeHpcCoreCopyEngineTests, givenCommandQueueWhenAskingForCacheFlushOnBcsThenReturnFalse) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto commandQueue = std::unique_ptr<CommandQueue>(CommandQueue::create(&context, clDevice.get(), nullptr, false, retVal));
    auto commandQueueHw = static_cast<CommandQueueHw<FamilyType> *>(commandQueue.get());

    EXPECT_FALSE(commandQueueHw->isCacheFlushForBcsRequired());
}
XE_HPC_CORETEST_F(XeHpcCoreCopyEngineTests, givenDebugFlagSetWhenCheckingBcsCacheFlushRequirementThenReturnCorrectValueForGen12p8) {
    DebugManagerStateRestore restorer;
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto commandQueue = std::unique_ptr<CommandQueue>(CommandQueue::create(&context, clDevice.get(), nullptr, false, retVal));
    auto commandQueueHw = static_cast<CommandQueueHw<FamilyType> *>(commandQueue.get());

    DebugManager.flags.ForceCacheFlushForBcs.set(0);
    EXPECT_FALSE(commandQueueHw->isCacheFlushForBcsRequired());

    DebugManager.flags.ForceCacheFlushForBcs.set(1);
    EXPECT_TRUE(commandQueueHw->isCacheFlushForBcsRequired());
}
