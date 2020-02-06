/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/command_queue_fixture.h"

#include "core/device/device.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/context/context.h"
#include "unit_tests/mocks/mock_device.h"

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
            this->device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)};
        }
        pDevice = this->device;
    }

    if (!context)
        context = new MockContext(pDevice);

    auto funcCreate = commandQueueFactory[pDevice->getRenderCoreFamily()];
    assert(nullptr != funcCreate);

    return funcCreate(context, pDevice, properties, false);
}

void CommandQueueHwFixture::SetUp() {
    ASSERT_NE(nullptr, pCmdQ);
    context = new MockContext();
}

void CommandQueueHwFixture::SetUp(
    ClDevice *pDevice,
    cl_command_queue_properties properties) {
    ASSERT_NE(nullptr, pDevice);
    context = new MockContext(pDevice);
    pCmdQ = createCommandQueue(pDevice, properties);
    ASSERT_NE(nullptr, pCmdQ);
}

void CommandQueueHwFixture::TearDown() {
    //resolve event dependencies
    if (pCmdQ) {
        auto blocked = pCmdQ->isQueueBlocked();
        UNRECOVERABLE_IF(blocked);
        pCmdQ->release();
    }
    if (context) {
        context->release();
    }
    if (device) {
        delete device;
    }
}

CommandQueueFixture::CommandQueueFixture()
    : pCmdQ(nullptr) {
}

CommandQueue *CommandQueueFixture::createCommandQueue(
    Context *context,
    ClDevice *device,
    cl_command_queue_properties properties) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, properties, 0};
    return new CommandQueue(
        context,
        device,
        props);
}

void CommandQueueFixture::SetUp(
    Context *context,
    ClDevice *device,
    cl_command_queue_properties properties) {
    pCmdQ = createCommandQueue(
        context,
        device,
        properties);
}

void CommandQueueFixture::TearDown() {
    delete pCmdQ;
    pCmdQ = nullptr;
}
} // namespace NEO
