/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/mocks/mock_kernel_info.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {
class MockKernel;
class MockProgram;
} // namespace NEO

using namespace NEO;

class KernelArgBufferFixture : public ContextFixture, public ClDeviceFixture {

    using ContextFixture::setUp;

  public:
    void setUp();
    void tearDown();

    cl_int retVal = CL_SUCCESS;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo = nullptr;
    char pSshLocal[64]{};
    char pCrossThreadData[64]{};
};
