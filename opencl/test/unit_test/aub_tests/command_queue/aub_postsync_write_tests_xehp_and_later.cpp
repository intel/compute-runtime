/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct PostSyncWriteXeHPTests : public HelloWorldFixture<AUBHelloWorldFixtureFactory>, public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(true);

        HelloWorldFixture<AUBHelloWorldFixtureFactory>::SetUp();
        EXPECT_TRUE(pCommandStreamReceiver->peekTimestampPacketWriteEnabled());
    };

    void TearDown() override {
        HelloWorldFixture<AUBHelloWorldFixtureFactory>::TearDown();
    }

    DebugManagerStateRestore restore;
    cl_int retVal = CL_SUCCESS;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, PostSyncWriteXeHPTests, givenTimestampWriteEnabledWhenEnqueueingThenWritePostsyncOperation) {
    MockCommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, nullptr);

    const uint32_t bufferSize = 4;

    std::unique_ptr<Buffer> buffer(Buffer::create(pContext, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    memset(graphicsAllocation->getUnderlyingBuffer(), 0, graphicsAllocation->getUnderlyingBufferSize());
    buffer->forceDisallowCPUCopy = true;

    uint8_t writeData[bufferSize] = {1, 2, 3, 4};
    cmdQ.enqueueWriteBuffer(buffer.get(), CL_TRUE, 0, bufferSize, writeData, nullptr, 0, nullptr, nullptr);
    expectMemory<FamilyType>(reinterpret_cast<void *>(graphicsAllocation->getGpuAddress()), writeData, bufferSize);

    typename FamilyType::TimestampPacketType expectedTimestampValues[4] = {1, 1, 1, 1};
    auto tagGpuAddress = reinterpret_cast<void *>(cmdQ.timestampPacketContainer->peekNodes().at(0)->getGpuAddress());
    expectMemoryNotEqual<FamilyType>(tagGpuAddress, expectedTimestampValues, 4 * sizeof(typename FamilyType::TimestampPacketType));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PostSyncWriteXeHPTests, givenDebugVariableEnabledWhenEnqueueingThenWritePostsyncOperationInImmWriteMode) {
    DebugManager.flags.UseImmDataWriteModeOnPostSyncOperation.set(true);
    MockCommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, nullptr);

    const uint32_t bufferSize = 4;

    std::unique_ptr<Buffer> buffer(Buffer::create(pContext, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    auto graphicsAllocation = buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex());
    memset(graphicsAllocation->getUnderlyingBuffer(), 0, graphicsAllocation->getUnderlyingBufferSize());
    buffer->forceDisallowCPUCopy = true;

    uint8_t writeData[bufferSize] = {1, 2, 3, 4};
    cmdQ.enqueueWriteBuffer(buffer.get(), CL_TRUE, 0, bufferSize, writeData, nullptr, 0, nullptr, nullptr);
    expectMemory<FamilyType>(reinterpret_cast<void *>(graphicsAllocation->getGpuAddress()), writeData, bufferSize);

    auto tagGpuAddress = reinterpret_cast<void *>(cmdQ.timestampPacketContainer->peekNodes().at(0)->getGpuAddress());

    constexpr auto timestampPacketTypeSize = sizeof(typename FamilyType::TimestampPacketType);
    if constexpr (timestampPacketTypeSize == 4u) {
        typename FamilyType::TimestampPacketType expectedTimestampValues[4] = {1, 1, 2, 2};
        expectMemory<FamilyType>(tagGpuAddress, expectedTimestampValues, 4 * timestampPacketTypeSize);
    } else {
        typename FamilyType::TimestampPacketType expectedTimestampValues[4] = {1, 1, 0x2'0000'0002u, 1};
        expectMemory<FamilyType>(tagGpuAddress, expectedTimestampValues, 4 * timestampPacketTypeSize);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PostSyncWriteXeHPTests, givenTwoBatchedEnqueuesWhenDependencyIsResolvedThenDecrementCounterOnGpu) {
    MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());
    pCommandStreamReceiver->overrideDispatchPolicy(DispatchMode::BatchedDispatch);

    const size_t bufferSize = 1024;
    auto retVal = CL_SUCCESS;
    uint8_t initialMemory[bufferSize] = {};
    uint8_t writePattern1[bufferSize];
    uint8_t writePattern2[bufferSize];
    std::fill(writePattern1, writePattern1 + sizeof(writePattern1), 1);
    std::fill(writePattern2, writePattern2 + sizeof(writePattern2), 1);

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(&context, CL_MEM_COPY_HOST_PTR, bufferSize, initialMemory, retVal));
    //make sure that GPU copy is used
    buffer->forceDisallowCPUCopy = true;
    cl_event outEvent1, outEvent2;

    pCmdQ->enqueueWriteBuffer(buffer.get(), CL_FALSE, 0, bufferSize, writePattern1, nullptr, 0, nullptr, &outEvent1);
    auto node1 = castToObject<Event>(outEvent1)->getTimestampPacketNodes()->peekNodes().at(0);
    node1->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()->setAubWritable(true, 0xffffffff); // allow to write again after Buffer::create

    pCmdQ->enqueueWriteBuffer(buffer.get(), CL_TRUE, 0, bufferSize, writePattern2, nullptr, 0, nullptr, &outEvent2);
    auto node2 = castToObject<Event>(outEvent2)->getTimestampPacketNodes()->peekNodes().at(0);

    expectMemory<FamilyType>(reinterpret_cast<void *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress()), writePattern2, bufferSize);

    typename FamilyType::TimestampPacketType expectedEndTimestamp = 1;
    auto endTimestampAddress1 = TimestampPacketHelper::getContextEndGpuAddress(*node1);
    auto endTimestampAddress2 = TimestampPacketHelper::getGlobalEndGpuAddress(*node1);
    auto endTimestampAddress3 = TimestampPacketHelper::getContextEndGpuAddress(*node2);
    auto endTimestampAddress4 = TimestampPacketHelper::getGlobalEndGpuAddress(*node2);
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(endTimestampAddress1), &expectedEndTimestamp, sizeof(typename FamilyType::TimestampPacketType));
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(endTimestampAddress2), &expectedEndTimestamp, sizeof(typename FamilyType::TimestampPacketType));
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(endTimestampAddress3), &expectedEndTimestamp, sizeof(typename FamilyType::TimestampPacketType));
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(endTimestampAddress4), &expectedEndTimestamp, sizeof(typename FamilyType::TimestampPacketType));

    clReleaseEvent(outEvent1);
    clReleaseEvent(outEvent2);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PostSyncWriteXeHPTests, givenMultipleWalkersWhenEnqueueingThenWriteAllTimestamps) {
    MockContext context(pCmdQ->getDevice().getSpecializedDevice<ClDevice>());
    const size_t bufferSize = 70;
    const size_t writeSize = bufferSize - 2;
    uint8_t writeData[writeSize] = {};
    cl_int retVal = CL_SUCCESS;
    cl_event outEvent;

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(&context, CL_MEM_READ_WRITE, bufferSize, nullptr, retVal));
    buffer->forceDisallowCPUCopy = true;

    pCmdQ->enqueueWriteBuffer(buffer.get(), CL_TRUE, 1, writeSize, writeData, nullptr, 0, nullptr, &outEvent);

    auto &timestampNodes = castToObject<Event>(outEvent)->getTimestampPacketNodes()->peekNodes();

    EXPECT_EQ(2u, timestampNodes.size());

    typename FamilyType::TimestampPacketType expectedEndTimestamp = 1;
    auto endTimestampAddress1 = TimestampPacketHelper::getContextEndGpuAddress(*timestampNodes[0]);
    auto endTimestampAddress2 = TimestampPacketHelper::getGlobalEndGpuAddress(*timestampNodes[0]);
    auto endTimestampAddress3 = TimestampPacketHelper::getContextEndGpuAddress(*timestampNodes[1]);
    auto endTimestampAddress4 = TimestampPacketHelper::getGlobalEndGpuAddress(*timestampNodes[1]);
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(endTimestampAddress1), &expectedEndTimestamp, sizeof(typename FamilyType::TimestampPacketType));
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(endTimestampAddress2), &expectedEndTimestamp, sizeof(typename FamilyType::TimestampPacketType));
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(endTimestampAddress3), &expectedEndTimestamp, sizeof(typename FamilyType::TimestampPacketType));
    expectMemoryNotEqual<FamilyType>(reinterpret_cast<void *>(endTimestampAddress4), &expectedEndTimestamp, sizeof(typename FamilyType::TimestampPacketType));

    clReleaseEvent(outEvent);
}
