/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"

namespace NEO {

class Kernel;
class Program;

struct HelloWorldKernelFixture : public ProgramFixture {
    using ProgramFixture::setUp;

    void setUp(ClDevice *pDevice, const char *kernelFilenameStr, const char *kernelNameStr) {
        FORBID_REAL_FILE_SYSTEM_CALLS();
        setUp(pDevice, kernelFilenameStr, kernelNameStr, nullptr);
    }
    void setUp(ClDevice *pDevice, const char *kernelFilenameStr, const char *kernelNameStr, const char *options) {
        ProgramFixture::setUp();

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

            createProgramFromBinary(
                pContext,
                deviceVector,
                *pTestFilename,
                optionsToProgram);
        } else {
            createProgramFromBinary(
                pContext,
                deviceVector,
                *pTestFilename);
        }

        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel, Program, MockMultiDeviceKernel>(
            pProgram,
            pProgram->getKernelInfosForKernel(pKernelName->c_str()),
            retVal);

        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(pDevice->getRootDeviceIndex()));
        EXPECT_NE(nullptr, pKernel);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    void setUp(ClDevice *pDevice, MockZebinWrapper<>::Descriptor desc) {
        ProgramFixture::setUp();
        pKernelName = new std::string("CopyBuffer");

        constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
        MockZebinWrapper<1u, numBits> zebin{pDevice->getHardwareInfo(), desc};

        auto deviceVector = toClDeviceVector(*pDevice);
        pContext = Context::create<MockContext>(nullptr, deviceVector, nullptr, nullptr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pContext);

        createProgramFromBinary(
            pContext,
            deviceVector,
            zebin.binaries.data(),
            zebin.binarySizes.data());

        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
        // create a kernel
        pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel, Program, MockMultiDeviceKernel>(
            pProgram,
            pProgram->getKernelInfosForKernel(pKernelName->c_str()),
            retVal);
        pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(pDevice->getRootDeviceIndex()));
        EXPECT_NE(nullptr, pKernel);
        EXPECT_EQ(CL_SUCCESS, retVal);
    }

    void setUp(ClDevice *pDevice) {
        auto productHelper = NEO::CompilerProductHelper::create(defaultHwInfo->platform.eProductFamily);
        MockZebinWrapper<>::Descriptor desc{};
        desc.isStateless = productHelper->isForceToStatelessRequired();

        setUp(pDevice, desc);
    }

    void tearDown() {
        delete pKernelName;
        delete pTestFilename;
        pMultiDeviceKernel->release();

        pContext->release();
        ProgramFixture::tearDown();
    }

    std::string *pTestFilename = nullptr;
    std::string *pKernelName = nullptr;
    cl_uint simd = 32;
    cl_int retVal = CL_SUCCESS;
    MockMultiDeviceKernel *pMultiDeviceKernel = nullptr;
    MockKernel *pKernel = nullptr;
    MockContext *pContext = nullptr;

    FORBID_REAL_FILE_SYSTEM_CALLS();
};
} // namespace NEO
