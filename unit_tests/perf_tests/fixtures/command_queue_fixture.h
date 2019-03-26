/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

namespace NEO {

class Context;
class Device;

struct CommandQueueHwFixture {
    CommandQueueHwFixture();

    CommandQueue *createCommandQueue(
        Context *context,
        Device *device,
        cl_command_queue_properties _properties = 0);

    virtual void SetUp();
    virtual void SetUp(Device *_pDevice, Context *context);

    virtual void TearDown();

    CommandQueue *pCmdQ;
};

struct CommandQueueFixture {
    CommandQueueFixture();

    virtual void SetUp(
        Context *context,
        Device *device,
        cl_command_queue_properties properties = 0);
    virtual void TearDown();

    CommandQueue *createCommandQueue(
        Context *context,
        Device *device,
        cl_command_queue_properties properties);

    CommandQueue *pCmdQ;
};

static const cl_command_queue_properties DefaultCommandQueueProperties[] = {
    0,
    CL_QUEUE_PROFILING_ENABLE,
};
} // namespace NEO
