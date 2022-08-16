/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_kernel_fixture.h"
#include "opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h"

namespace NEO {

// Factory used to pick various ingredients for use in aggregate tests
struct HelloWorldFixtureFactory {
    typedef NEO::IndirectHeapFixture IndirectHeapFixture;
    typedef NEO::CommandStreamFixture CommandStreamFixture;
    typedef NEO::CommandQueueHwFixture CommandQueueFixture;
    typedef NEO::HelloWorldKernelFixture KernelFixture;
};

//  Instantiates a fixture based on the supplied fixture factory.
//  Used by most tests for integration testing with command queues.
template <typename FixtureFactory>
struct HelloWorldFixture : public FixtureFactory::IndirectHeapFixture,
                           public FixtureFactory::CommandStreamFixture,
                           public FixtureFactory::CommandQueueFixture,
                           public FixtureFactory::KernelFixture,
                           public ClDeviceFixture {

    typedef typename FixtureFactory::IndirectHeapFixture IndirectHeapFixture;
    typedef typename FixtureFactory::CommandStreamFixture CommandStreamFixture;
    typedef typename FixtureFactory::CommandQueueFixture CommandQueueFixture;
    typedef typename FixtureFactory::KernelFixture KernelFixture;

    using CommandQueueFixture::pCmdQ;
    using CommandQueueFixture::setUp;
    using CommandStreamFixture::pCS;
    using CommandStreamFixture::setUp;
    using HelloWorldKernelFixture::setUp;
    using IndirectHeapFixture::setUp;
    using KernelFixture::pKernel;

  public:
    void setUp() {
        ClDeviceFixture::setUp();
        ASSERT_NE(nullptr, pClDevice);
        CommandQueueFixture::setUp(pClDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
        CommandStreamFixture::setUp(pCmdQ);
        ASSERT_NE(nullptr, pCS);
        IndirectHeapFixture::setUp(pCmdQ);
        KernelFixture::setUp(pClDevice, kernelFilename, kernelName);
        ASSERT_NE(nullptr, pKernel);

        auto retVal = CL_INVALID_VALUE;
        BufferDefaults::context = new MockContext(pClDevice);

        destBuffer = Buffer::create(
            BufferDefaults::context,
            CL_MEM_READ_WRITE,
            sizeUserMemory,
            nullptr,
            retVal);

        srcBuffer = Buffer::create(
            BufferDefaults::context,
            CL_MEM_READ_WRITE,
            sizeUserMemory,
            nullptr,
            retVal);

        pDestMemory = destBuffer->getCpuAddressForMapping();
        pSrcMemory = srcBuffer->getCpuAddressForMapping();

        memset(pDestMemory, destPattern, sizeUserMemory);
        memset(pSrcMemory, srcPattern, sizeUserMemory);

        pKernel->setArg(0, srcBuffer);
        pKernel->setArg(1, destBuffer);
    }

    void tearDown() {
        pCmdQ->flush();

        srcBuffer->release();
        destBuffer->release();

        KernelFixture::tearDown();
        IndirectHeapFixture::tearDown();
        CommandStreamFixture::tearDown();
        CommandQueueFixture::tearDown();
        BufferDefaults::context->release();
        ClDeviceFixture::tearDown();
    }
    Buffer *srcBuffer = nullptr;
    Buffer *destBuffer = nullptr;
    void *pSrcMemory = nullptr;
    void *pDestMemory = nullptr;
    size_t sizeUserMemory = 128 * sizeof(float);
    const char *kernelFilename = "CopyBuffer_simd";
    const char *kernelName = "CopyBuffer";
    const int srcPattern = 85;
    const int destPattern = 170;

    cl_int callOneWorkItemNDRKernel(cl_event *eventWaitList = nullptr, cl_int waitListSize = 0, cl_event *returnEvent = nullptr) {

        cl_uint workDim = 1;
        size_t globalWorkOffset[3] = {0, 0, 0};
        size_t globalWorkSize[3] = {1, 1, 1};
        size_t localWorkSize[3] = {1, 1, 1};

        return pCmdQ->enqueueKernel(
            pKernel,
            workDim,
            globalWorkOffset,
            globalWorkSize,
            localWorkSize,
            waitListSize,
            eventWaitList,
            returnEvent);
    }
};

template <typename FixtureFactory>
struct HelloWorldTest : Test<HelloWorldFixture<FixtureFactory>> {
};

template <typename FixtureFactory>
struct HelloWorldTestWithParam : HelloWorldFixture<FixtureFactory> {
};
} // namespace NEO
