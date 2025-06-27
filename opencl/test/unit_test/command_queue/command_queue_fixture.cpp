/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"

#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "gtest/gtest.h"

namespace NEO {

// Global table of create functions
extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];

CommandQueue *CommandQueueHwFixture::createCommandQueue(
    ClDevice *pDevice,
    cl_command_queue_properties properties) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, properties, 0};

    return createCommandQueue(pDevice, props);
}

CommandQueue *CommandQueueHwFixture::createCommandQueue(
    ClDevice *pDevice,
    const cl_command_queue_properties *properties) {
    if (pDevice == nullptr) {
        if (this->device == nullptr) {
            this->device = new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
            createdDevice = true;
        }
        pDevice = this->device;
    }

    if (!context) {
        context = new MockContext(pDevice);
    }
    return createCommandQueue(pDevice, properties, context);
}

CommandQueue *CommandQueueHwFixture::createCommandQueue(
    ClDevice *pDevice,
    const cl_command_queue_properties *properties,
    Context *pContext) {
    auto funcCreate = commandQueueFactory[pDevice->getRenderCoreFamily()];
    assert(nullptr != funcCreate);

    return funcCreate(pContext, pDevice, properties, false);
}

void CommandQueueHwFixture::forceMapBufferOnGpu(Buffer &buffer) {
    ClDevice *clDevice = buffer.getContext()->getDevice(0);
    buffer.setSharingHandler(new SharingHandler());
    auto gfxAllocation = buffer.getGraphicsAllocation(clDevice->getRootDeviceIndex());
    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        gfxAllocation->setGmm(new MockGmm(clDevice->getGmmHelper()), handleId);
    }
}

void CommandQueueHwFixture::setUp() {
    ASSERT_NE(nullptr, pCmdQ);
    context = new MockContext();
}

void CommandQueueHwFixture::setUp(
    ClDevice *pDevice,
    cl_command_queue_properties properties) {
    ASSERT_NE(nullptr, pDevice);
    context = new MockContext(pDevice);
    pCmdQ = createCommandQueue(pDevice, properties);
    ASSERT_NE(nullptr, pCmdQ);
}

void CommandQueueHwFixture::tearDown() {
    // resolve event dependencies
    if (pCmdQ) {
        auto blocked = pCmdQ->isQueueBlocked();
        UNRECOVERABLE_IF(blocked);
        pCmdQ->release();
    }
    if (context) {
        context->release();
    }
    if (createdDevice) {
        delete device;
    }
}

CommandQueue *CommandQueueFixture::createCommandQueue(
    Context *context,
    ClDevice *device,
    cl_command_queue_properties properties,
    bool internalUsage) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, properties, 0};
    return new MockCommandQueue(
        context,
        device,
        props,
        internalUsage);
}

void CommandQueueFixture::setUp(
    Context *context,
    ClDevice *device,
    cl_command_queue_properties properties) {
    pCmdQ = createCommandQueue(
        context,
        device,
        properties,
        false);
}

void CommandQueueFixture::tearDown() {
    delete pCmdQ;
    pCmdQ = nullptr;
}

void OOQueueFixture ::setUp(ClDevice *pDevice, cl_command_queue_properties properties) {
    ASSERT_NE(nullptr, pDevice);
    BaseClass::pCmdQ = BaseClass::createCommandQueue(pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
    ASSERT_NE(nullptr, BaseClass::pCmdQ);
}

void CommandQueueHwTest::SetUp() {
    ClDeviceFixture::setUp();
    cl_device_id device = pClDevice;
    ContextFixture::setUp(1, &device);
    CommandQueueHwFixture::setUp(pClDevice, 0);
}

void CommandQueueHwTest::TearDown() {
    CommandQueueHwFixture::tearDown();
    ContextFixture::tearDown();
    ClDeviceFixture::tearDown();
}

void OOQueueHwTest::SetUp() {
    ClDeviceFixture::setUp();
    cl_device_id device = pClDevice;
    ContextFixture::setUp(1, &device);
    OOQueueFixture::setUp(pClDevice, 0);
}

void OOQueueHwTest::TearDown() {
    OOQueueFixture::tearDown();
    ContextFixture::tearDown();
    ClDeviceFixture::tearDown();
}

} // namespace NEO
