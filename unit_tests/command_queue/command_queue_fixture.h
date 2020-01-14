/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue.h"
#include "unit_tests/mocks/mock_context.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

namespace NEO {
class Device;

struct CommandQueueHwFixture {
    CommandQueue *createCommandQueue(ClDevice *device) {
        return createCommandQueue(device, cl_command_queue_properties{0});
    }

    CommandQueue *createCommandQueue(
        ClDevice *device,
        cl_command_queue_properties properties);

    CommandQueue *createCommandQueue(
        ClDevice *device,
        const cl_command_queue_properties *properties);

    virtual void SetUp();
    virtual void SetUp(ClDevice *_pDevice, cl_command_queue_properties properties);

    virtual void TearDown();

    CommandQueue *pCmdQ = nullptr;
    ClDevice *device = nullptr;
    MockContext *context = nullptr;
};

struct OOQueueFixture : public CommandQueueHwFixture {
    typedef CommandQueueHwFixture BaseClass;

    virtual void SetUp(ClDevice *_pDevice, cl_command_queue_properties _properties) override {
        ASSERT_NE(nullptr, _pDevice);
        BaseClass::pCmdQ = BaseClass::createCommandQueue(_pDevice, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE);
        ASSERT_NE(nullptr, BaseClass::pCmdQ);
    }
};

struct CommandQueueFixture {
    CommandQueueFixture();

    virtual void SetUp(
        Context *context,
        ClDevice *device,
        cl_command_queue_properties properties);
    virtual void TearDown();

    CommandQueue *createCommandQueue(
        Context *context,
        ClDevice *device,
        cl_command_queue_properties properties);

    CommandQueue *pCmdQ;
};

static const cl_command_queue_properties AllCommandQueueProperties[] = {
    0,
    CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT,
    CL_QUEUE_PROFILING_ENABLE,
    CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
    CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_ON_DEVICE_DEFAULT};

static const cl_command_queue_properties DefaultCommandQueueProperties[] = {
    0,
    CL_QUEUE_PROFILING_ENABLE,
};
} // namespace NEO
