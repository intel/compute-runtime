/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

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
    using ProgramFixture::setUp;

    void setUp(ClDevice *pDevice, const char *kernelFilenameStr, const char *kernelNameStr) {
        USE_REAL_FILE_SYSTEM();
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

    void setUp(ClDevice *pDevice, const char *kernelNameStr) {
        ProgramFixture::setUp();
        pKernelName = new std::string(kernelNameStr);

        constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
        auto zebinData = std::make_unique<ZebinTestData::ZebinCopyBufferSimdModule<numBits>>(pDevice->getHardwareInfo(), 32);
        const auto &src = zebinData->storage;

        ASSERT_NE(nullptr, src.data());
        ASSERT_NE(0u, src.size());

        const unsigned char *binaries[1] = {reinterpret_cast<const unsigned char *>(src.data())};
        const size_t binarySize = src.size();

        auto deviceVector = toClDeviceVector(*pDevice);
        pContext = Context::create<MockContext>(nullptr, deviceVector, nullptr, nullptr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pContext);

        createProgramFromBinary(
            pContext,
            deviceVector,
            binaries,
            &binarySize);

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
};
} // namespace NEO
