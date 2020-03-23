/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"

namespace NEO {

void api_fixture_using_aligned_memory_manager::SetUp() {
    retVal = CL_SUCCESS;
    retSize = 0;

    device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockAlignedMallocManagerDevice>(defaultHwInfo.get())};
    cl_device_id deviceId = device;

    context = Context::create<MockContext>(nullptr, ClDeviceVector(&deviceId, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    Context *ctxPtr = reinterpret_cast<Context *>(context);

    commandQueue = new MockCommandQueue(context, device, 0);

    program = new MockProgram(*device->getExecutionEnvironment(), ctxPtr, false, &device->getDevice());
    Program *prgPtr = reinterpret_cast<Program *>(program);

    kernel = new MockKernel(prgPtr, program->mockKernelInfo, *device);
    ASSERT_NE(nullptr, kernel);
}

void api_fixture_using_aligned_memory_manager::TearDown() {
    delete kernel;
    delete commandQueue;
    context->release();
    program->release();
    delete device;
}
} // namespace NEO
