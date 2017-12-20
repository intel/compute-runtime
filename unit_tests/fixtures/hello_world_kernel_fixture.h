/*
 * Copyright (c) 2017, Intel Corporation
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
#include "gtest/gtest.h"
#include "CL/cl.h"
#include "runtime/device/device.h"
#include "runtime/helpers/file_io.h"
#include "runtime/kernel/kernel.h"
#include "runtime/platform/platform.h"
#include "runtime/program/program.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"

namespace OCLRT {

class Kernel;
class Program;

struct HelloWorldKernelFixture : public ProgramFixture {
    using ProgramFixture::SetUp;

    virtual void SetUp(Device *pDevice, const char *kernelFilenameStr, const char *kernelNameStr, const char *options = nullptr) {
        ProgramFixture::SetUp();

        pTestFilename = new std::string(kernelFilenameStr);
        pKernelName = new std::string(kernelNameStr);

        if (strstr(kernelFilenameStr, "_simd") != nullptr) {
            pTestFilename->append(std::to_string(simd));
        }

        cl_device_id device = pDevice;
        pContext = Context::create<MockContext>(nullptr, DeviceVector(&device, 1), nullptr, nullptr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pContext);

        if (options) {
            std::string optionsToProgram(options);

            if (optionsToProgram.find("-cl-std=CL2.0") != std::string::npos) {
                ASSERT_TRUE(pDevice->getSupportedClVersion() >= 20u);
            }

            CreateProgramFromBinary<Program>(
                pContext,
                &device,
                *pTestFilename,
                optionsToProgram);
        } else {
            CreateProgramFromBinary<Program>(
                pContext,
                &device,
                *pTestFilename);
        }

        ASSERT_NE(nullptr, pProgram);

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
            *pProgram->getKernelInfo(pKernelName->c_str()),
            &retVal);

        EXPECT_NE(nullptr, pKernel);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    virtual void TearDown() {
        delete pKernelName;
        delete pTestFilename;
        delete pKernel;
        pKernel = nullptr;

        pContext->release();
        ProgramFixture::TearDown();
    }

    std::string *pTestFilename = nullptr;
    std::string *pKernelName = nullptr;
    cl_uint simd = 32;
    cl_int retVal = CL_SUCCESS;
    Kernel *pKernel = nullptr;
    MockContext *pContext = nullptr;
};
} // namespace OCLRT
