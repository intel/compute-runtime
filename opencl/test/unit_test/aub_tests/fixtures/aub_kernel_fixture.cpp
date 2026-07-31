/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/fixtures/aub_kernel_fixture.h"

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/test/common/helpers/mock_file_io.h"
#include "shared/test/common/helpers/test_files.h"

#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/program/create.inl"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

ReleaseableObjectPtr<MockProgram> createProgramFromBinaryFile(Context *pContext,
                                                              const std::string &binaryFileName) {
    std::string testFile;
    retrieveBinaryKernelFilename(testFile, binaryFileName + "_", ".bin");

    size_t knownSourceSize = 0u;
    auto knownSource = loadDataFromVirtualFileTestKernelsOnly(testFile.c_str(), knownSourceSize);
    EXPECT_NE(0u, knownSourceSize);
    EXPECT_NE(nullptr, knownSource);
    if (knownSourceSize == 0u || knownSource == nullptr) {
        return nullptr;
    }

    cl_device_id rootDeviceId = pContext->getDevices()[0];
    ClDeviceVector rootDeviceVector(&rootDeviceId, 1);

    cl_int retVal = CL_SUCCESS;
    auto program = Program::create<MockProgram>(
        pContext,
        rootDeviceVector,
        &knownSourceSize,
        (const unsigned char **)&knownSource,
        nullptr,
        retVal);
    EXPECT_NE(nullptr, program);
    EXPECT_EQ(CL_SUCCESS, retVal);
    return clUniquePtr(program);
}

void SimpleArgIntKernelFixture::setUp(MockContext *context) {
    pProgram = createProgramFromBinaryFile(context, "simple_kernels");
    ASSERT_NE(nullptr, pProgram);

    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // create a kernel
    pKernel.reset(Kernel::create<MockKernel>(
        pProgram.get(),
        pProgram->getKernelInfoForKernel("simple_arg_int"),
        *context->getDevices()[0],
        retVal));

    ASSERT_NE(nullptr, pKernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

void SimpleArgIntKernelFixture::tearDown() {
}

void SimpleArgNonUniformKernelFixture::setUp(ClDevice *device, Context *context) {
    pProgram = createProgramFromBinaryFile(context, "simple_nonuniform");
    ASSERT_NE(nullptr, pProgram);
    pProgram->allowNonUniform = true;

    retVal = pProgram->build(
        pProgram->getDevices(),
        "-cl-std=CL2.0");
    ASSERT_EQ(CL_SUCCESS, retVal);

    kernel.reset(Kernel::create<MockKernel>(
        pProgram.get(),
        pProgram->getKernelInfoForKernel("simpleNonUniform"),
        *device,
        retVal));
    ASSERT_NE(nullptr, kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

void SimpleArgNonUniformKernelFixture::tearDown() {
}

void SimpleKernelFixture::setUp(ClDevice *device, Context *context) {
    pProgram = createProgramFromBinaryFile(context, "simple_kernels");
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
                pProgram.get(),
                pProgram->getKernelInfoForKernel(kernelName.c_str()),
                *device,
                retVal));
            ASSERT_NE(nullptr, kernels[i]);
            ASSERT_EQ(CL_SUCCESS, retVal);
        }
    }
}

void SimpleKernelFixture::tearDown() {
}

void SimpleKernelStatelessFixture::setUp(ClDevice *device, Context *context) {
    debugManager.flags.DisableStatelessToStatefulOptimization.set(true);
    debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.set(false);

    pProgram = createProgramFromBinaryFile(context, "stateless_kernel");
    ASSERT_NE(nullptr, pProgram);

    retVal = pProgram->build(
        pProgram->getDevices(),
        CompilerOptions::greaterThan4gbBuffersRequired.data());
    ASSERT_EQ(CL_SUCCESS, retVal);

    kernel.reset(Kernel::create<MockKernel>(
        pProgram.get(),
        pProgram->getKernelInfoForKernel("statelessKernel"),
        *device,
        retVal));
    ASSERT_NE(nullptr, kernel);
    ASSERT_EQ(CL_SUCCESS, retVal);
}

void SimpleKernelStatelessFixture::tearDown() {
}

void AUBHelloWorldKernelFixture::setUp(MockContext *context, const char *kernelFilenameStr, const char *kernelNameStr) {
    std::string testFilename(kernelFilenameStr);
    std::string kernelName(kernelNameStr);

    if (strstr(kernelFilenameStr, "_simd") != nullptr) {
        testFilename.append(std::to_string(simd));
    }

    pProgram = createProgramFromBinaryFile(context, testFilename);
    ASSERT_NE(nullptr, pProgram);

    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // create a kernel
    pMultiDeviceKernel.reset(MultiDeviceKernel::create<MockKernel, Program, MockMultiDeviceKernel>(
        pProgram.get(),
        pProgram->getKernelInfosForKernel(kernelName.c_str()),
        retVal));

    pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(context->getDevices()[0]->getRootDeviceIndex()));
    EXPECT_NE(nullptr, pKernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

void AUBHelloWorldKernelFixture::tearDown() {
}

} // namespace NEO
