/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/file_io.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/device/device.h"
#include "runtime/helpers/array_count.h"
#include "runtime/kernel/kernel.h"
#include "runtime/program/program.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

#include "CL/cl.h"
#include "compiler_options.h"
#include "gtest/gtest.h"

#include <type_traits>

namespace NEO {

class Kernel;
class Program;

template <typename T>
inline const char *type_name(T &) {
    return "unknown";
}

template <>
inline const char *type_name(char &) {
    return "char";
}

template <>
inline const char *type_name(int &) {
    return "int";
}

template <>
inline const char *type_name(float &) {
    return "float";
}

template <>
inline const char *type_name(short &) {
    return "short";
}

template <>
inline const char *type_name(unsigned char &) {
    return "unsigned char";
}

template <>
inline const char *type_name(unsigned int &) {
    return "unsigned int";
}

template <>
inline const char *type_name(unsigned short &) {
    return "unsigned short";
}

class SimpleArgKernelFixture : public ProgramFixture {

  public:
    using ProgramFixture::SetUp;
    SimpleArgKernelFixture()
        : retVal(CL_SUCCESS), pKernel(nullptr) {
    }

  protected:
    virtual void SetUp(ClDevice *pDevice) {
        ProgramFixture::SetUp();

        std::string testFile;
        int forTheName = 0;

        testFile.append("simple_arg_");
        testFile.append(type_name(forTheName));

        auto pos = testFile.find(" ");
        if (pos != (size_t)-1) {
            testFile.replace(pos, 1, "_");
        }

        cl_device_id device = pDevice;
        pContext = Context::create<MockContext>(nullptr, ClDeviceVector(&device, 1), nullptr, nullptr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pContext);

        CreateProgramFromBinary(
            pContext,
            &device,
            testFile);
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
            *pProgram->getKernelInfo("SimpleArg"),
            &retVal);

        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    virtual void TearDown() {
        if (pKernel) {
            delete pKernel;
            pKernel = nullptr;
        }

        pContext->release();

        ProgramFixture::TearDown();
    }

    cl_int retVal;
    Kernel *pKernel;
    MockContext *pContext;
};

class SimpleArgNonUniformKernelFixture : public ProgramFixture {
  public:
    using ProgramFixture::SetUp;
    SimpleArgNonUniformKernelFixture()
        : retVal(CL_SUCCESS), kernel(nullptr) {
    }

  protected:
    void SetUp(ClDevice *device, Context *context) {
        ProgramFixture::SetUp();

        cl_device_id deviceId = device;
        cl_context clContext = context;

        CreateProgramFromBinary(
            clContext,
            &deviceId,
            "simple_nonuniform",
            "-cl-std=CL2.0");
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            1,
            &deviceId,
            "-cl-std=CL2.0",
            nullptr,
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel = Kernel::create<MockKernel>(
            pProgram,
            *pProgram->getKernelInfo("simpleNonUniform"),
            &retVal);
        ASSERT_NE(nullptr, kernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    virtual void TearDown() {
        if (kernel) {
            delete kernel;
            kernel = nullptr;
        }

        ProgramFixture::TearDown();
    }

    cl_int retVal;
    Kernel *kernel;
};

class SimpleKernelFixture : public ProgramFixture {
  public:
    using ProgramFixture::SetUp;

  protected:
    void SetUp(ClDevice *device, Context *context) {
        ProgramFixture::SetUp();

        cl_device_id deviceId = device;
        cl_context clContext = context;
        std::string programName("simple_kernels");
        CreateProgramFromBinary(
            clContext,
            &deviceId,
            programName);
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            1,
            &deviceId,
            nullptr,
            nullptr,
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        for (size_t i = 0; i < maxKernelsCount; i++) {
            if ((1 << i) & kernelIds) {
                std::string kernelName("simple_kernel_");
                kernelName.append(std::to_string(i));
                kernels[i].reset(Kernel::create<MockKernel>(
                    pProgram,
                    *pProgram->getKernelInfo(kernelName.c_str()),
                    &retVal));
                ASSERT_NE(nullptr, kernels[i]);
                ASSERT_EQ(CL_SUCCESS, retVal);
            }
        }
    }

    virtual void TearDown() {
        for (size_t i = 0; i < maxKernelsCount; i++) {
            if (kernels[i]) {
                kernels[i].reset(nullptr);
            }
        }

        ProgramFixture::TearDown();
    }

    uint32_t kernelIds = 0;
    static constexpr size_t maxKernelsCount = std::numeric_limits<decltype(kernelIds)>::digits;
    cl_int retVal = CL_SUCCESS;
    std::array<std::unique_ptr<Kernel>, maxKernelsCount> kernels;
};

class SimpleKernelStatelessFixture : public ProgramFixture {
  public:
    DebugManagerStateRestore restorer;
    using ProgramFixture::SetUp;
    SimpleKernelStatelessFixture() = default;

  protected:
    void SetUp(ClDevice *device, Context *context) {
        ProgramFixture::SetUp();
        cl_device_id deviceId = device;
        cl_context clContext = context;
        DebugManager.flags.DisableStatelessToStatefulOptimization.set(true);
        DebugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(false);

        CreateProgramFromBinary(
            clContext,
            &deviceId,
            "stateless_kernel");
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            1,
            &deviceId,
            CompilerOptions::greaterThan4gbBuffersRequired,
            nullptr,
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel.reset(Kernel::create<MockKernel>(
            pProgram,
            *pProgram->getKernelInfo("statelessKernel"),
            &retVal));
        ASSERT_NE(nullptr, kernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void TearDown() {
        ProgramFixture::TearDown();
    }

    std::unique_ptr<Kernel> kernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};

} // namespace NEO
