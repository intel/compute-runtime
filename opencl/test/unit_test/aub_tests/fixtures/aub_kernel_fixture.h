/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <limits>
#include <string>

namespace NEO {

class MockContext;

ReleaseableObjectPtr<MockProgram> createProgramFromBinaryFile(Context *pContext,
                                                              const std::string &binaryFileName);

struct SimpleArgIntKernelFixture {
    void setUp(MockContext *context);
    void tearDown();

    cl_int retVal = CL_SUCCESS;
    ReleaseableObjectPtr<MockProgram> pProgram;
    std::unique_ptr<Kernel> pKernel;
};

struct SimpleArgNonUniformKernelFixture {
    void setUp(ClDevice *device, Context *context);
    void tearDown();

    cl_int retVal = CL_SUCCESS;
    ReleaseableObjectPtr<MockProgram> pProgram;
    std::unique_ptr<Kernel> kernel;
};

struct SimpleKernelFixture {
    void setUp(ClDevice *device, Context *context);
    void tearDown();

    uint32_t kernelIds = 0;
    static constexpr size_t maxKernelsCount = std::numeric_limits<decltype(kernelIds)>::digits;
    cl_int retVal = CL_SUCCESS;
    ReleaseableObjectPtr<MockProgram> pProgram;
    std::array<std::unique_ptr<Kernel>, maxKernelsCount> kernels;
};

struct SimpleKernelStatelessFixture {
    void setUp(ClDevice *device, Context *context);
    void tearDown();

    DebugManagerStateRestore restorer;
    std::unique_ptr<Kernel> kernel;
    cl_int retVal = CL_SUCCESS;
    ReleaseableObjectPtr<MockProgram> pProgram;
};

struct AUBHelloWorldKernelFixture {
    void setUp(MockContext *context, const char *kernelFilenameStr, const char *kernelNameStr);

    void tearDown();

    cl_uint simd = 32;
    cl_int retVal = CL_SUCCESS;
    ReleaseableObjectPtr<MockProgram> pProgram;
    ReleaseableObjectPtr<MockMultiDeviceKernel> pMultiDeviceKernel;
    MockKernel *pKernel = nullptr;
};

struct SimpleArgFixture : public IndirectHeapFixture,
                          public AUBCommandStreamFixture,
                          public SimpleArgIntKernelFixture {

    using AUBCommandStreamFixture::context;
    using AUBCommandStreamFixture::pCmdQ;
    using AUBCommandStreamFixture::pCS;
    using AUBCommandStreamFixture::setUp;
    using IndirectHeapFixture::setUp;
    using SimpleArgIntKernelFixture::pKernel;
    using SimpleArgIntKernelFixture::setUp;

    void setUp() {
        AUBCommandStreamFixture::setUp();
        ASSERT_NE(nullptr, pCS);
        IndirectHeapFixture::setUp(pCmdQ);
        SimpleArgIntKernelFixture::setUp(context);
        ASSERT_NE(nullptr, pKernel);

        argVal = static_cast<int>(0x22222222);
        pDestMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pDestMemory);

        pExpectedMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pExpectedMemory);

        memset(pDestMemory, 0x11, sizeUserMemory);
        memset(pExpectedMemory, 0x22, sizeUserMemory);

        pKernel->setArg(0, sizeof(int), &argVal);
        pKernel->setArgSvm(1, sizeUserMemory, pDestMemory, nullptr, 0u);

        outBuffer = AUBCommandStreamFixture::createResidentAllocationAndStoreItInCsr(pDestMemory, sizeUserMemory);
        ASSERT_NE(nullptr, outBuffer);
        outBuffer->setAllocationType(AllocationType::buffer);
        outBuffer->setMemObjectsAllocationWithWritableFlags(true);
    }

    void tearDown() {
        if (pExpectedMemory) {
            alignedFree(pExpectedMemory);
            pExpectedMemory = nullptr;
        }
        if (pDestMemory) {
            alignedFree(pDestMemory);
            pDestMemory = nullptr;
        }

        SimpleArgIntKernelFixture::tearDown();
        IndirectHeapFixture::tearDown();
        AUBCommandStreamFixture::tearDown();
    }

    int argVal = 0;
    void *pDestMemory = nullptr;
    void *pExpectedMemory = nullptr;
    size_t sizeUserMemory = 128 * sizeof(float);
    GraphicsAllocation *outBuffer = nullptr;
};
} // namespace NEO
