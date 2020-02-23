/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/fixtures/device_host_queue_fixture.h"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_device_queue.h"

using namespace NEO;
using namespace DeviceHostQueue;

typedef DeviceQueueHwTest Gen8DeviceQueueSlb;

GEN8TEST_F(Gen8DeviceQueueSlb, expectedAllocationSize) {
    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    auto expectedSize = getMinimumSlbSize<FamilyType>() +
                        sizeof(typename FamilyType::MI_ATOMIC) +
                        sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM) +
                        sizeof(typename FamilyType::MI_LOAD_REGISTER_IMM);
    expectedSize *= 128; //num of enqueues
    expectedSize += sizeof(typename FamilyType::MI_BATCH_BUFFER_START);
    expectedSize = alignUp(expectedSize, MemoryConstants::pageSize);
    expectedSize += MockDeviceQueueHw<FamilyType>::getExecutionModelCleanupSectionSize();
    expectedSize += (4 * MemoryConstants::pageSize);
    expectedSize = alignUp(expectedSize, MemoryConstants::pageSize);
    ASSERT_NE(deviceQueue->getSlbBuffer(), nullptr);
    EXPECT_EQ(deviceQueue->getSlbBuffer()->getUnderlyingBufferSize(), expectedSize);

    delete deviceQueue;
}

GEN8TEST_F(Gen8DeviceQueueSlb, SlbCommandsWa) {
    auto mockDeviceQueueHw = new MockDeviceQueueHw<FamilyType>(pContext, device,
                                                               DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);

    EXPECT_FALSE(mockDeviceQueueHw->arbCheckWa);
    EXPECT_FALSE(mockDeviceQueueHw->pipeControlWa);
    EXPECT_TRUE(mockDeviceQueueHw->miAtomicWa);
    EXPECT_TRUE(mockDeviceQueueHw->lriWa);

    delete mockDeviceQueueHw;
}

GEN8TEST_F(Gen8DeviceQueueSlb, addProfilingEndcmds) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto mockDeviceQueueHw = new MockDeviceQueueHw<FamilyType>(pContext, device,
                                                               DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);

    uint64_t timestampAddress = 0x12345678555500;

    uint32_t timestampAddressLow = (uint32_t)(timestampAddress & 0xFFFFFFFF);
    uint32_t timestampAddressHigh = (uint32_t)(timestampAddress >> 32);

    mockDeviceQueueHw->addProfilingEndCmds(timestampAddress);

    HardwareParse hwParser;
    auto *slbCS = mockDeviceQueueHw->getSlbCS();

    hwParser.parseCommands<FamilyType>(*slbCS, 0);

    auto pipeControlItor = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), pipeControlItor);

    PIPE_CONTROL *pipeControl = (PIPE_CONTROL *)*pipeControlItor;

    uint32_t postSyncOp = (uint32_t)PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP;
    EXPECT_EQ(postSyncOp, (uint32_t)pipeControl->getPostSyncOperation());

    EXPECT_EQ(timestampAddressLow, pipeControl->getAddress());
    EXPECT_EQ(timestampAddressHigh, pipeControl->getAddressHigh());

    delete mockDeviceQueueHw;
}
