/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include <array>
#include <memory>

namespace NEO {
class ClDevice;
class Context;
class MockContext;
class MockProgram;

std::unique_ptr<MockKernelWithInternals> createBufferArgsKernel(Context &context);
std::unique_ptr<MockKernelWithInternals> createSimpleArgKernel(Context &context);
std::unique_ptr<MockKernelWithInternals> createBufferArgsKernelWithRequiredWorkGroupSize(Context &context, std::array<uint16_t, 3> requiredWorkGroupSize);

struct MockKernelFixture {
    void setUp(ClDevice *pDevice);
    void tearDown();

    ReleaseableObjectPtr<MockContext> pContext;
    ReleaseableObjectPtr<MockProgram> pProgram;
    ReleaseableObjectPtr<MockMultiDeviceKernel> pMultiDeviceKernelOwner;
    MockMultiDeviceKernel *pMultiDeviceKernel = nullptr;
    MockKernel *pKernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};
} // namespace NEO
