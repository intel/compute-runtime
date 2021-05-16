/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/program/program_from_binary.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"
#include "test.h"

using namespace NEO;

struct ExecutionModelKernelFixture : public ProgramFromBinaryFixture,
                                     public PlatformFixture {
    using ProgramFromBinaryFixture::SetUp;
    void SetUp(const char *binaryFileName, const char *kernelName) {
        REQUIRE_DEVICE_ENQUEUE_OR_SKIP(defaultHwInfo);

        PlatformFixture::SetUp();

        std::string options("-cl-std=CL2.0");
        this->setOptions(options);
        ProgramFromBinaryFixture::SetUp(binaryFileName, kernelName);

        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(pProgram,
                                                                   pProgram->getKernelInfosForKernel(kernelName),
                                                                   &retVal);
        pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pKernel);
    }

    void TearDown() override {
        if (IsSkipped()) {
            return;
        }
        if (pMultiDeviceKernel != nullptr) {
            pMultiDeviceKernel->release();
        }

        ProgramFromBinaryFixture::TearDown();
        PlatformFixture::TearDown();

        if (pDevice != nullptr) {
            delete pDevice;
            pDevice = nullptr;
        }
        if (pClDevice != nullptr) {
            delete pClDevice;
            pClDevice = nullptr;
        }
    }

    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    Kernel *pKernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};
