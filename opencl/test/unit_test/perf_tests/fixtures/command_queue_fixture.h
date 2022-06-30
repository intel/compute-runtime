/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/command_queue/command_queue.h"

#include "CL/cl.h"

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
