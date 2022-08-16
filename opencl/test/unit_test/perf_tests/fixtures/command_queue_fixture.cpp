/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/perf_tests/fixtures/command_queue_fixture.h"

#include "shared/source/device/device.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/context/context.h"

#include "gtest/gtest.h"

namespace NEO {

// Global table of create functions
extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];

CommandQueueHwFixture::CommandQueueHwFixture()
    : pCmdQ(nullptr) {
}

CommandQueue *CommandQueueHwFixture::createCommandQueue(
    Context *context,
    Device *pDevice,
    cl_command_queue_properties properties) {

    auto funcCreate = commandQueueFactory[pDevice->getHardwareInfo().platform->eRenderCoreFamily];
    assert(nullptr != funcCreate);

    return funcCreate(context, pDevice, properties);
}

void CommandQueueHwFixture::setUp() {
    ASSERT_NE(nullptr, pCmdQ);
}

void CommandQueueHwFixture::setUp(
    Device *pDevice, Context *context) {
    ASSERT_NE(nullptr, pDevice);
    pCmdQ = createCommandQueue(context, pDevice);
    CommandQueueHwFixture::setUp();
}

void CommandQueueHwFixture::tearDown() {
    delete pCmdQ;
}

CommandQueueFixture::CommandQueueFixture()
    : pCmdQ(nullptr) {
}

CommandQueue *CommandQueueFixture::createCommandQueue(
    Context *context,
    Device *device,
    cl_command_queue_properties properties) {
    return new CommandQueue(
        context,
        device,
        properties);
}

void CommandQueueFixture::setUp(
    Context *context,
    Device *device,
    cl_command_queue_properties properties) {
    pCmdQ = createCommandQueue(
        context,
        device,
        properties);
}

void CommandQueueFixture::tearDown() {
    delete pCmdQ;
    pCmdQ = nullptr;
}
} // namespace NEO
