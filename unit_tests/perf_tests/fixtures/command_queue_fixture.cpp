/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "unit_tests/perf_tests/fixtures/command_queue_fixture.h"

#include "gtest/gtest.h"

namespace OCLRT {

// Global table of create functions
extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];

CommandQueueHwFixture::CommandQueueHwFixture()
    : pCmdQ(nullptr) {
}

CommandQueue *CommandQueueHwFixture::createCommandQueue(
    Context *context,
    Device *pDevice,
    cl_command_queue_properties properties) {

    auto funcCreate = commandQueueFactory[pDevice->getHardwareInfo().pPlatform->eRenderCoreFamily];
    assert(nullptr != funcCreate);

    return funcCreate(context, pDevice, properties);
}

void CommandQueueHwFixture::SetUp() {
    ASSERT_NE(nullptr, pCmdQ);
}

void CommandQueueHwFixture::SetUp(
    Device *pDevice, Context *context) {
    ASSERT_NE(nullptr, pDevice);
    pCmdQ = createCommandQueue(context, pDevice);
    CommandQueueHwFixture::SetUp();
}

void CommandQueueHwFixture::TearDown() {
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
