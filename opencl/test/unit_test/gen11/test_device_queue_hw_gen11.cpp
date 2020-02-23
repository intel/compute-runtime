/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/fixtures/device_host_queue_fixture.h"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_device_queue.h"

#include <memory>

using namespace NEO;
using namespace DeviceHostQueue;

typedef DeviceQueueHwTest Gen11DeviceQueueSlb;

GEN11TEST_F(Gen11DeviceQueueSlb, expectedAllocationSize) {
    deviceQueue = createQueueObject();
    ASSERT_NE(deviceQueue, nullptr);

    auto expectedSize = getMinimumSlbSize<FamilyType>();
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

GEN11TEST_F(Gen11DeviceQueueSlb, SlbCommandsWa) {
    auto mockDeviceQueueHw = std::make_unique<MockDeviceQueueHw<FamilyType>>(pContext, device,
                                                                             DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);
    EXPECT_FALSE(mockDeviceQueueHw->arbCheckWa);
    EXPECT_FALSE(mockDeviceQueueHw->pipeControlWa);
    EXPECT_FALSE(mockDeviceQueueHw->miAtomicWa);
    EXPECT_FALSE(mockDeviceQueueHw->lriWa);
}

GEN11TEST_F(Gen11DeviceQueueSlb, GivenDeviceQueueWhenSingleEnqueueSpaceIsNotCachelineAlignedThenCSPrefetchIsExtendedWithCachelineAlignement) {
    auto mockDeviceQueueHw = std::make_unique<MockDeviceQueueHw<FamilyType>>(pContext, device,
                                                                             DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);

    EXPECT_LE(8 * MemoryConstants::cacheLineSize, mockDeviceQueueHw->getCSPrefetchSize());
    EXPECT_EQ(0u, (mockDeviceQueueHw->getMinimumSlbSize() & (MemoryConstants::cacheLineSize - 1)));
}
