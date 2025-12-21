/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_queue/command_enqueue_fixture.h"

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

namespace NEO {

void CommandDeviceFixture::setUp(cl_command_queue_properties cmdQueueProperties) {
    ClDeviceFixture::setUp();
    CommandQueueHwFixture::setUp(pClDevice, cmdQueueProperties);
}

void NegativeFailAllocationCommandEnqueueBaseFixture::setUp() {
    CommandEnqueueBaseFixture::setUp();
    failMemManager.reset(new FailMemoryManager(*pDevice->getExecutionEnvironment()));

    BufferDefaults::context = context;
    Image2dDefaults::context = context;
    buffer.reset(BufferHelper<>::create());
    image.reset(ImageHelperUlt<Image2dDefaults>::create());
    ptr = static_cast<void *>(array);
    oldMemManager = pDevice->getExecutionEnvironment()->memoryManager.release();
    pDevice->injectMemoryManager(failMemManager.release());
}

void NegativeFailAllocationCommandEnqueueBaseFixture::tearDown() {
    pDevice->injectMemoryManager(oldMemManager);
    buffer.reset(nullptr);
    image.reset(nullptr);
    BufferDefaults::context = nullptr;
    Image2dDefaults::context = nullptr;
    CommandEnqueueBaseFixture::tearDown();
}

} // namespace NEO
