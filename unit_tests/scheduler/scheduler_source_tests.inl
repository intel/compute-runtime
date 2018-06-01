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

#include "runtime/device_queue/device_queue.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"
#include "unit_tests/mocks/mock_device_queue.h"

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
