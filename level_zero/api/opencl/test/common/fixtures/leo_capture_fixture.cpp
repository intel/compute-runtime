/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"

namespace NEO {
namespace LEO {
namespace ult {

void LeoCaptureFixture::setUp() {
    OclFixture::setUp();

    auto &clDevices = platform->getDevices();
    ASSERT_FALSE(clDevices.empty());
    clDevice = clDevices[0].get();

    cl_int errcode = CL_SUCCESS;
    cl_device_id clDeviceId = clDevice;
    clContext = clCreateContext(nullptr, 1, &clDeviceId, nullptr, nullptr, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    ASSERT_NE(nullptr, clContext);

    context = castToObject<Context>(clContext);
    ASSERT_NE(nullptr, context);

    commandQueue = new CommandQueue(context, clDevice, nullptr, capturingCmdList.toHandle());
}

void LeoCaptureFixture::tearDown() {
    if (commandQueue != nullptr) {
        commandQueue->decRefApi();
        commandQueue = nullptr;
    }
    if (clContext != nullptr) {
        clReleaseContext(clContext);
        clContext = nullptr;
    }
    OclFixture::tearDown();
}

cl_mem LeoCaptureFixture::createBuffer(size_t size, cl_mem_flags flags) {
    cl_int errcode = CL_SUCCESS;
    auto buffer = clCreateBuffer(clContext, flags, size, nullptr, &errcode);
    EXPECT_EQ(CL_SUCCESS, errcode);
    EXPECT_NE(nullptr, buffer);
    return buffer;
}

} // namespace ult
} // namespace LEO
} // namespace NEO
