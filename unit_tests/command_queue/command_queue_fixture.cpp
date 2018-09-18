/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/command_queue_fixture.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "hw_cmds.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "gtest/gtest.h"

namespace OCLRT {

// Global table of create functions
extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];

CommandQueueHwFixture::CommandQueueHwFixture()
    : pCmdQ(nullptr), context(nullptr) {
}

CommandQueue *CommandQueueHwFixture::createCommandQueue(
    Device *pDevice,
    cl_command_queue_properties properties) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, properties, 0};

    auto funcCreate = commandQueueFactory[pDevice->getRenderCoreFamily()];
    assert(nullptr != funcCreate);
    if (!context)
        context = new MockContext(pDevice);

    return funcCreate(context, pDevice, props);
}

void CommandQueueHwFixture::SetUp() {
    ASSERT_NE(nullptr, pCmdQ);
    context = new MockContext();
}

void CommandQueueHwFixture::SetUp(
    Device *pDevice,
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
    context->release();
}

CommandQueueFixture::CommandQueueFixture()
    : pCmdQ(nullptr) {
}

CommandQueue *CommandQueueFixture::createCommandQueue(
    Context *context,
    Device *device,
    cl_command_queue_properties properties) {
    const cl_queue_properties props[3] = {CL_QUEUE_PROPERTIES, properties, 0};
    return new CommandQueue(
        context,
        device,
        props);
}

void CommandQueueFixture::SetUp(
    Context *context,
    Device *device,
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
} // namespace OCLRT
