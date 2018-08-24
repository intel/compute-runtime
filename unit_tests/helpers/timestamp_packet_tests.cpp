/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/timestamp_packet.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"

#include "test.h"

using namespace OCLRT;

class MockTimestampPacket : public TimestampPacket {
  public:
    using TimestampPacket::data;
};

using TimestampPacketTests = ::testing::Test;

TEST_F(TimestampPacketTests, whenObjectIsCreatedThenInitializeAllStamps) {
    MockTimestampPacket timestampPacket;
    auto maxElements = static_cast<uint32_t>(TimestampPacket::DataIndex::Max);
    EXPECT_EQ(4u, maxElements);

    EXPECT_EQ(maxElements, timestampPacket.data.size());

    for (uint32_t i = 0; i < maxElements; i++) {
        EXPECT_EQ(1u, timestampPacket.pickDataValue(static_cast<TimestampPacket::DataIndex>(i)));
        EXPECT_EQ(1u, timestampPacket.data[i]);
    }
}

TEST_F(TimestampPacketTests, whenAskedForStampAddressThenReturnWithValidOffset) {
    MockTimestampPacket timestampPacket;

    EXPECT_EQ(&timestampPacket.data[0], timestampPacket.pickDataPtr());

    auto startAddress = timestampPacket.pickAddressForPipeControlWrite(TimestampPacket::WriteOperationType::Start);
    auto expectedStartAddress = &timestampPacket.data[static_cast<uint32_t>(TimestampPacket::DataIndex::ContextStart)];
    EXPECT_EQ(expectedStartAddress, &timestampPacket.data[0]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(expectedStartAddress), startAddress);

    auto endAddress = timestampPacket.pickAddressForPipeControlWrite(TimestampPacket::WriteOperationType::End);
    auto expectedEndAddress = &timestampPacket.data[static_cast<uint32_t>(TimestampPacket::DataIndex::ContextEnd)];
    EXPECT_EQ(expectedEndAddress, &timestampPacket.data[2]);
    EXPECT_EQ(reinterpret_cast<uint64_t>(expectedEndAddress), endAddress);
}

HWTEST_F(TimestampPacketTests, givenDebugVariableEnabledWhenEstimatingStreamSizeThenAddTwoPipeControls) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableTimestampPacket.set(false);

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockCommandQueue cmdQ(nullptr, device.get(), nullptr);
    MockKernelWithInternals kernel1(*device);
    MockKernelWithInternals kernel2(*device);
    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel1.mockKernel, kernel2.mockKernel}));

    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(cmdQ, false, false, multiDispatchInfo);
    auto sizeWithDisabled = cmdQ.requestedCmdStreamSize;

    DebugManager.flags.EnableTimestampPacket.set(true);
    getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(cmdQ, false, false, multiDispatchInfo);
    auto sizeWithEnabled = cmdQ.requestedCmdStreamSize;

    EXPECT_EQ(sizeWithEnabled, sizeWithDisabled + 2 * sizeof(typename FamilyType::PIPE_CONTROL));
}

HWCMDTEST_F(IGFX_GEN8_CORE, TimestampPacketTests, givenEnabledDebugVariableWhenDispatchingGpuWalkerThenAddTwoPcForLastWalker) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockTimestampPacket timestampPacket;
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableTimestampPacket.set(true);

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel1(*device);
    MockKernelWithInternals kernel2(*device);

    MockMultiDispatchInfo multiDispatchInfo(std::vector<Kernel *>({kernel1.mockKernel, kernel2.mockKernel}));

    MockCommandQueue cmdQ(nullptr, device.get(), nullptr);
    auto &cmdStream = cmdQ.getCS(0);

    GpgpuWalkerHelper<FamilyType>::dispatchWalker(
        cmdQ,
        multiDispatchInfo,
        0,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        &timestampPacket,
        device->getPreemptionMode(),
        false);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, 0);

    auto verifyPipeControl = [](PIPE_CONTROL *pipeControl, uint64_t expectedAddress) {
        EXPECT_EQ(1u, pipeControl->getCommandStreamerStallEnable());
        EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControl->getPostSyncOperation());
        EXPECT_EQ(0u, pipeControl->getImmediateData());
        EXPECT_EQ(static_cast<uint32_t>(expectedAddress & 0x0000FFFFFFFFULL), pipeControl->getAddress());
        EXPECT_EQ(static_cast<uint32_t>(expectedAddress >> 32), pipeControl->getAddressHigh());
    };

    uint32_t walkersFound = 0;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        if (genCmdCast<GPGPU_WALKER *>(*it)) {
            walkersFound++;
            if (walkersFound == 1) {
                EXPECT_EQ(nullptr, genCmdCast<PIPE_CONTROL *>(*--it));
                it++;
                EXPECT_EQ(nullptr, genCmdCast<PIPE_CONTROL *>(*++it));
                it--;
            } else if (walkersFound == 2) {
                auto pipeControl = genCmdCast<PIPE_CONTROL *>(*--it);
                EXPECT_NE(nullptr, pipeControl);
                verifyPipeControl(pipeControl, timestampPacket.pickAddressForPipeControlWrite(TimestampPacket::WriteOperationType::Start));
                it++;
                pipeControl = genCmdCast<PIPE_CONTROL *>(*++it);
                EXPECT_NE(nullptr, pipeControl);
                verifyPipeControl(pipeControl, timestampPacket.pickAddressForPipeControlWrite(TimestampPacket::WriteOperationType::End));
                it--;
            }
        }
    }
    EXPECT_EQ(2u, walkersFound);
}
