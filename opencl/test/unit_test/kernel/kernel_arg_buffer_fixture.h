/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class KernelArgBufferFixture : public ContextFixture, public DeviceFixture {

    using ContextFixture::SetUp;

  public:
    KernelArgBufferFixture()
        : retVal(CL_SUCCESS), pProgram(nullptr), pKernel(nullptr), pKernelInfo(nullptr) {
    }

  protected:
    void SetUp();
    void TearDown();

    cl_int retVal;
    MockProgram *pProgram;
    MockKernel *pKernel;
    std::unique_ptr<KernelInfo> pKernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    char pSshLocal[64];
    char pCrossThreadData[64];
};
