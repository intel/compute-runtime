/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/kernel/kernel.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/program/program_from_binary.h"

using namespace NEO;

class ExecutionModelKernelFixture : public ProgramFromBinaryTest,
                                    public PlatformFixture {
  public:
    ExecutionModelKernelFixture() : pKernel(nullptr),
                                    retVal(CL_SUCCESS) {
    }

    ~ExecutionModelKernelFixture() override = default;

  protected:
    void SetUp() override {
        PlatformFixture::SetUp();

        std::string temp;
        temp.assign(pPlatform->getDevice(0)->getDeviceInfo().clVersion);

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
        temp.assign(pPlatform->getDevice(0)->getDeviceInfo().clVersion);

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

    Kernel *pKernel;
    cl_int retVal;
};
