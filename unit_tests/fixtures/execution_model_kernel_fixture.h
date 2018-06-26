/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/program/program_from_binary.h"
#include "unit_tests/mocks/mock_kernel.h"

#include "test.h"

using namespace OCLRT;

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
            pDevice = DeviceHelper<>::create();
            return;
        }

        std::string options("-cl-std=CL2.0");
        this->setOptions(options);
        ProgramFromBinaryTest::SetUp();

        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        cl_device_id device = pDevice;
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
        delete pKernel;

        std::string temp;
        temp.assign(pPlatform->getDevice(0)->getDeviceInfo().clVersion);

        if (temp.find("OpenCL 1.2") != std::string::npos) {
            delete pDevice;
            pDevice = nullptr;
        }
        ProgramFromBinaryTest::TearDown();
        PlatformFixture::TearDown();
    }

    Kernel *pKernel;
    cl_int retVal;
};
