/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/file_io.h"
#include "runtime/device/device.h"
#include "runtime/kernel/kernel.h"
#include "runtime/platform/platform.h"
#include "runtime/program/program.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

namespace NEO {

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

            CreateProgramFromBinary(
                pContext,
                &device,
                *pTestFilename,
                optionsToProgram);
        } else {
            CreateProgramFromBinary(
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
        pKernel->release();

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
} // namespace NEO
