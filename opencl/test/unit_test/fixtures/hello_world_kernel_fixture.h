/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"

namespace NEO {

class Kernel;
class Program;

struct HelloWorldKernelFixture : public ProgramFixture {
    using ProgramFixture::SetUp;

    void SetUp(ClDevice *pDevice, const char *kernelFilenameStr, const char *kernelNameStr) {
        SetUp(pDevice, kernelFilenameStr, kernelNameStr, nullptr);
    }
    void SetUp(ClDevice *pDevice, const char *kernelFilenameStr, const char *kernelNameStr, const char *options) {
        ProgramFixture::SetUp();

        pTestFilename = new std::string(kernelFilenameStr);
        pKernelName = new std::string(kernelNameStr);

        if (strstr(kernelFilenameStr, "_simd") != nullptr) {
            pTestFilename->append(std::to_string(simd));
        }

        auto deviceVector = toClDeviceVector(*pDevice);
        pContext = Context::create<MockContext>(nullptr, deviceVector, nullptr, nullptr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pContext);

        if (options) {
            std::string optionsToProgram(options);

            CreateProgramFromBinary(
                pContext,
                deviceVector,
                *pTestFilename,
                optionsToProgram);
        } else {
            CreateProgramFromBinary(
                pContext,
                deviceVector,
                *pTestFilename);
        }

        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel, Program, MockMultiDeviceKernel>(
            pProgram,
            pProgram->getKernelInfosForKernel(pKernelName->c_str()),
            &retVal);

        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(pDevice->getRootDeviceIndex()));
        EXPECT_NE(nullptr, pKernel);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() override {
        delete pKernelName;
        delete pTestFilename;
        pMultiDeviceKernel->release();

        pContext->release();
        ProgramFixture::TearDown();
    }

    std::string *pTestFilename = nullptr;
    std::string *pKernelName = nullptr;
    cl_uint simd = 32;
    cl_int retVal = CL_SUCCESS;
    MockMultiDeviceKernel *pMultiDeviceKernel = nullptr;
    MockKernel *pKernel = nullptr;
    MockContext *pContext = nullptr;
};
} // namespace NEO
