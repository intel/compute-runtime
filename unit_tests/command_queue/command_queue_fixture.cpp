/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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
