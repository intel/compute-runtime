/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/program/program_from_binary.h"
#include "test.h"

using namespace NEO;

class ExecutionModelKernelFixture : public ProgramFromBinaryTest,
                                    public PlatformFixture {
  protected:
    void SetUp() override {
        PlatformFixture::SetUp();

        std::string temp;
        temp.assign(pPlatform->getClDevice(0)->getDeviceInfo().clVersion);

        if (temp.find("OpenCL 1.2") != std::string::npos) {
            pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
            pClDevice = new MockClDevice{pDevice};
            return;
        }

        std::string options("-cl-std=CL2.0");
        this->setOptions(options);
        ProgramFromBinaryTest::SetUp();

        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        cl_device_id device = pClDevice;
        retVal = pProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        pKernel = Kernel::create<MockKernel>(
            pProgram,
            *pProgram->getKernelInfo(KernelName),
            &retVal);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pKernel);
    }

    void TearDown() override {
        if (pKernel != nullptr) {
            pKernel->release();
        }

        std::string temp;
        temp.assign(pPlatform->getClDevice(0)->getDeviceInfo().clVersion);

        ProgramFromBinaryTest::TearDown();
        PlatformFixture::TearDown();

        if (temp.find("OpenCL 1.2") != std::string::npos) {
            if (pDevice != nullptr) {
                delete pDevice;
                pDevice = nullptr;
            }
            if (pClDevice != nullptr) {
                delete pClDevice;
                pClDevice = nullptr;
            }
        }
    }

    Kernel *pKernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};
