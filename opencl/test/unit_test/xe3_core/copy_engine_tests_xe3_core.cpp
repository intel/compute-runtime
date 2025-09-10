/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
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

#include "hw_cmds_xe3_core.h"

using namespace NEO;

struct BlitXe3CoreTests : public ::testing::Test {
    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        debugManager.flags.RenderCompressedBuffersEnabled.set(true);
        debugManager.flags.EnableLocalMemory.set(true);
        HardwareInfo hwInfo = *defaultHwInfo;
        hwInfo.capabilityTable.blitterOperationsSupported = true;

        clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    }

    std::optional<TaskCountType> flushBcsTask(CommandStreamReceiver *csr, const BlitProperties &blitProperties, bool blocking, Device &device) {
        BlitPropertiesContainer blitPropertiesContainer;
        blitPropertiesContainer.push_back(blitProperties);

        return csr->flushBcsTask(blitPropertiesContainer, blocking, device);
    }

    std::unique_ptr<MockClDevice> clDevice;
    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    DebugManagerStateRestore debugRestorer;
};

XE3_CORETEST_F(BlitXe3CoreTests, givenBufferWhenProgrammingBltCommandThenSetMocs) {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()), 0,
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()), 0,
                                                                     0, 0, {1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto itorBltCmd = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorBltCmd);
    MEM_COPY *bltCmd = (MEM_COPY *)*itorBltCmd;

    if (clDevice->getProductHelper().deferMOCSToPatIndex(clDevice->getRootDeviceEnvironment().isWddmOnLinux())) {
        EXPECT_EQ(0u, bltCmd->getDestinationMOCS());
        EXPECT_EQ(0u, bltCmd->getSourceMOCS());
    } else {
        auto mocsL3Enabled = 0x10u;
        EXPECT_EQ(mocsL3Enabled, bltCmd->getDestinationMOCS());
        EXPECT_EQ(mocsL3Enabled, bltCmd->getSourceMOCS());
    }
}

XE3_CORETEST_F(BlitXe3CoreTests, givenBufferWhenProgrammingBltCommandThenSetMocsToValueOfDebugKey) {
    DebugManagerStateRestore restorer;
    uint32_t expectedMocs = 0;
    debugManager.flags.OverrideBlitterMocs.set(expectedMocs);
    using MEM_COPY = typename FamilyType::MEM_COPY;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());
    MockGraphicsAllocation clearColorAlloc;

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto blitProperties = BlitProperties::constructPropertiesForCopy(buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()), 0,
                                                                     buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex()), 0,
                                                                     0, 0, {1, 1, 1}, 0, 0, 0, 0, &clearColorAlloc);

    flushBcsTask(csr, blitProperties, true, clDevice->getDevice());

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream);

    auto itorBltCmd = find<MEM_COPY *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorBltCmd);
    MEM_COPY *bltCmd = (MEM_COPY *)*itorBltCmd;

    EXPECT_EQ(expectedMocs, bltCmd->getDestinationMOCS());
    EXPECT_EQ(expectedMocs, bltCmd->getSourceMOCS());
}

XE3_CORETEST_F(BlitXe3CoreTests, given2dBlitCommandWhenDispatchingThenSetValidSurfaceType) {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    auto &bcsEngine = clDevice->getEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine.commandStreamReceiver);
    MockContext context(clDevice.get());

    cl_int retVal = CL_SUCCESS;
    auto buffer = clUniquePtr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto allocation = buffer->getGraphicsAllocation(clDevice->getRootDeviceIndex());
    MockGraphicsAllocation clearColorAlloc;

    size_t offset = 0;
    {
        // 1D
        auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, 0, allocation, 0,
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
    }

    {
        // 2D
        auto blitProperties = BlitProperties::constructPropertiesForCopy(allocation, 0, allocation, 0,
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
