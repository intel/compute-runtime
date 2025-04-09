/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"

#include <type_traits>

namespace NEO {

class Kernel;
class Program;

class SimpleArgKernelFixture : public ProgramFixture {

  public:
    using ProgramFixture::setUp;

  protected:
    void setUp(ClDevice *pDevice) {
        ProgramFixture::setUp();

        auto deviceVector = toClDeviceVector(*pDevice);
        pContext = Context::create<MockContext>(nullptr, deviceVector, nullptr, nullptr, retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pContext);

        createProgramFromBinary(
            pContext,
            deviceVector,
            "simple_kernels");
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        pKernel = Kernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfoForKernel("simple_arg_int"),
            *pDevice,
            retVal);

        ASSERT_NE(nullptr, pKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void tearDown() {
        if (pKernel) {
            delete pKernel;
            pKernel = nullptr;
        }

        pContext->release();

        ProgramFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    Kernel *pKernel = nullptr;
    MockContext *pContext = nullptr;
};

class SimpleArgNonUniformKernelFixture : public ProgramFixture {
  public:
    using ProgramFixture::setUp;

  protected:
    void setUp(ClDevice *device, Context *context) {
        USE_REAL_FILE_SYSTEM();
        ProgramFixture::setUp();

        createProgramFromBinary(
            context,
            context->getDevices(),
            "simple_nonuniform",
            "");
        ASSERT_NE(nullptr, pProgram);
        pProgram->allowNonUniform = true;

        retVal = pProgram->build(
            pProgram->getDevices(),
            "-cl-std=CL2.0");
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel = Kernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfoForKernel("simpleNonUniform"),
            *device,
            retVal);
        ASSERT_NE(nullptr, kernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void tearDown() {
        if (kernel) {
            delete kernel;
            kernel = nullptr;
        }

        ProgramFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    Kernel *kernel = nullptr;
};

class SimpleKernelFixture : public ProgramFixture {
  public:
    using ProgramFixture::setUp;

  protected:
    void setUp(ClDevice *device, Context *context) {
        ProgramFixture::setUp();

        std::string programName("simple_kernels");
        createProgramFromBinary(
            context,
            toClDeviceVector(*device),
            programName);
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        for (size_t i = 0; i < maxKernelsCount; i++) {
            if ((1 << i) & kernelIds) {
                std::string kernelName("simple_kernel_");
                kernelName.append(std::to_string(i));
                kernels[i].reset(Kernel::create<MockKernel>(
                    pProgram,
                    pProgram->getKernelInfoForKernel(kernelName.c_str()),
                    *device,
                    retVal));
                ASSERT_NE(nullptr, kernels[i]);
                ASSERT_EQ(CL_SUCCESS, retVal);
            }
        }
    }

    void tearDown() {
        for (size_t i = 0; i < maxKernelsCount; i++) {
            if (kernels[i]) {
                kernels[i].reset(nullptr);
            }
        }

        ProgramFixture::tearDown();
    }

    uint32_t kernelIds = 0;
    static constexpr size_t maxKernelsCount = std::numeric_limits<decltype(kernelIds)>::digits;
    cl_int retVal = CL_SUCCESS;
    std::array<std::unique_ptr<Kernel>, maxKernelsCount> kernels;
};

class SimpleKernelStatelessFixture : public ProgramFixture {
  public:
    DebugManagerStateRestore restorer;
    using ProgramFixture::setUp;

  protected:
    void setUp(ClDevice *device, Context *context) {
        ProgramFixture::setUp();
        debugManager.flags.DisableStatelessToStatefulOptimization.set(true);
        debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(false);

        createProgramFromBinary(
            context,
            toClDeviceVector(*device),
            "stateless_kernel");
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            CompilerOptions::greaterThan4gbBuffersRequired.data());
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel.reset(Kernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfoForKernel("statelessKernel"),
            *device,
            retVal));
        ASSERT_NE(nullptr, kernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void tearDown() {
        ProgramFixture::tearDown();
    }

    std::unique_ptr<Kernel> kernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};

class StatelessCopyKernelFixture : public ProgramFixture {
  public:
    DebugManagerStateRestore restorer;
    using ProgramFixture::setUp;

  protected:
    void setUp(ClDevice *device, Context *context) {
        ProgramFixture::setUp();
        debugManager.flags.DisableStatelessToStatefulOptimization.set(true);
        debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(false);

        createProgramFromBinary(
            context,
            toClDeviceVector(*device),
            "stateless_copy_buffer");
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            CompilerOptions::greaterThan4gbBuffersRequired.data());
        ASSERT_EQ(CL_SUCCESS, retVal);

        multiDeviceKernel.reset(MultiDeviceKernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfosForKernel("StatelessCopyBuffer"),
            retVal));
        kernel = static_cast<MockKernel *>(multiDeviceKernel->getKernel(device->getRootDeviceIndex()));
        ASSERT_NE(nullptr, kernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void tearDown() {
        ProgramFixture::tearDown();
    }

    std::unique_ptr<MultiDeviceKernel> multiDeviceKernel = nullptr;
    MockKernel *kernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};

class StatelessKernelWithIndirectAccessFixture : public ProgramFixture {
  public:
    DebugManagerStateRestore restorer;
    using ProgramFixture::setUp;

  protected:
    void setUp(ClDevice *device, Context *context) {
        ProgramFixture::setUp();
        debugManager.flags.DisableStatelessToStatefulOptimization.set(true);
        debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(false);

        createProgramFromBinary(
            context,
            toClDeviceVector(*device),
            "indirect_access_kernel");
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            CompilerOptions::greaterThan4gbBuffersRequired.data());
        ASSERT_EQ(CL_SUCCESS, retVal);

        multiDeviceKernel.reset(MultiDeviceKernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfosForKernel("testIndirect"),
            retVal));
        ASSERT_NE(nullptr, multiDeviceKernel);
        ASSERT_EQ(CL_SUCCESS, retVal);

        EXPECT_TRUE(multiDeviceKernel->getKernel(device->getRootDeviceIndex())->getKernelInfo().kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess);
    }

    void tearDown() {
        ProgramFixture::tearDown();
    }

    std::unique_ptr<MultiDeviceKernel> multiDeviceKernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};

class BindlessKernelFixture : public ProgramFixture {
  public:
    using ProgramFixture::setUp;
    void setUp(ClDevice *device, Context *context) {
        ProgramFixture::setUp();
        this->deviceCl = device;
        this->contextCl = context;
    }

    void tearDown() {
        ProgramFixture::tearDown();
    }

    void createKernel(const std::string &programName, const std::string &kernelName) {
        debugManager.flags.UseBindlessMode.set(1);
        createProgramFromBinary(
            contextCl,
            contextCl->getDevices(),
            programName);
        ASSERT_NE(nullptr, pProgram);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        kernel.reset(Kernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfoForKernel(kernelName.c_str()),
            *deviceCl,
            retVal));
        ASSERT_NE(nullptr, kernel);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Kernel> kernel = nullptr;
    cl_int retVal = CL_SUCCESS;
    ClDevice *deviceCl = nullptr;
    Context *contextCl = nullptr;
};

} // namespace NEO
