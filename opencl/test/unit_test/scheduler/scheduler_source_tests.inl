/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/device_queue/device_queue.h"
#include "opencl/test/unit_test/fixtures/device_host_queue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_device_queue.h"

#include <memory>

template <typename GfxFamily>
void SchedulerSourceTest::givenDeviceQueueWhenCommandsSizeIsCalculatedThenItEqualsSpaceForEachEnqueueInSchedulerKernelCodeTest() {
    auto devQueueHw = std::unique_ptr<MockDeviceQueueHw<GfxFamily>>(new MockDeviceQueueHw<GfxFamily>(&context, pDevice, DeviceHostQueue::deviceQueueProperties::minimumProperties[0]));

    auto singleEnqueueSpace = devQueueHw->getMinimumSlbSize() + devQueueHw->getWaCommandsSize();
    EXPECT_EQ(singleEnqueueSpace, SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE);
}

template <typename GfxFamily>
void SchedulerSourceTest::givenDeviceQueueWhenSlbDummyCommandsAreBuildThenSizeUsedIsCorrectTest() {
    auto devQueueHw = std::unique_ptr<MockDeviceQueueHw<GfxFamily>>(new MockDeviceQueueHw<GfxFamily>(&context, pDevice, DeviceHostQueue::deviceQueueProperties::minimumProperties[0]));
    devQueueHw->buildSlbDummyCommands();

    auto slbCS = devQueueHw->getSlbCS();
    auto usedSpace = slbCS->getUsed();

    auto spaceRequiredForEnqueuesAndBBStart = SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE * SECOND_LEVEL_BUFFER_NUMBER_OF_ENQUEUES + sizeof(typename GfxFamily::MI_BATCH_BUFFER_START);
    EXPECT_EQ(usedSpace, spaceRequiredForEnqueuesAndBBStart);
}

template <typename GfxFamily>
void SchedulerSourceTest::givenDeviceQueueThenNumberOfEnqueuesEqualsNumberOfEnqueuesInSchedulerKernelCodeTest() {
    EXPECT_EQ(DeviceQueue::numberOfDeviceEnqueues, static_cast<uint32_t>(SECOND_LEVEL_BUFFER_NUMBER_OF_ENQUEUES));
}
