/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/memory_manager.h"
#include "test.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/hello_world_kernel_fixture.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"

namespace OCLRT {

// Factory used to pick various ingredients for use in aggregate tests
struct HelloWorldFixtureFactory {
    typedef OCLRT::IndirectHeapFixture IndirectHeapFixture;
    typedef OCLRT::CommandStreamFixture CommandStreamFixture;
    typedef OCLRT::CommandQueueHwFixture CommandQueueFixture;
    typedef OCLRT::HelloWorldKernelFixture KernelFixture;
};

//  Instantiates a fixture based on the supplied fixture factory.
//  Used by most tests for integration testing with command queues.
template <typename FixtureFactory>
struct HelloWorldFixture : public FixtureFactory::IndirectHeapFixture,
                           public FixtureFactory::CommandStreamFixture,
                           public FixtureFactory::CommandQueueFixture,
                           public FixtureFactory::KernelFixture,
                           public DeviceFixture {

    typedef typename FixtureFactory::IndirectHeapFixture IndirectHeapFixture;
    typedef typename FixtureFactory::CommandStreamFixture CommandStreamFixture;
    typedef typename FixtureFactory::CommandQueueFixture CommandQueueFixture;
    typedef typename FixtureFactory::KernelFixture KernelFixture;

    using CommandQueueFixture::pCmdQ;
    using CommandQueueFixture::SetUp;
    using CommandStreamFixture::pCS;
    using CommandStreamFixture::SetUp;
    using HelloWorldKernelFixture::SetUp;
    using IndirectHeapFixture::SetUp;
    using KernelFixture::pKernel;

    HelloWorldFixture() : pSrcMemory(nullptr),
                          pDestMemory(nullptr),
                          sizeUserMemory(128 * sizeof(float)),
                          kernelFilename("CopyBuffer_simd"),
                          kernelName("CopyBuffer") {
    }

  public:
    virtual void SetUp() {
        DeviceFixture::SetUp();
        ASSERT_NE(nullptr, pDevice);
        CommandQueueFixture::SetUp(pDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
        CommandStreamFixture::SetUp(pCmdQ);
        ASSERT_NE(nullptr, pCS);
        IndirectHeapFixture::SetUp(pCmdQ);
        KernelFixture::SetUp(pDevice, kernelFilename, kernelName);
        ASSERT_NE(nullptr, pKernel);

        auto retVal = CL_INVALID_VALUE;
        BufferDefaults::context = new MockContext(pDevice);

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

    virtual void TearDown() {
        pCmdQ->flush();

        srcBuffer->release();
        destBuffer->release();

        KernelFixture::TearDown();
        IndirectHeapFixture::TearDown();
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        BufferDefaults::context->release();
        DeviceFixture::TearDown();
    }
    Buffer *srcBuffer;
    Buffer *destBuffer;
    void *pSrcMemory;
    void *pDestMemory;
    size_t sizeUserMemory;
    const char *kernelFilename;
    const char *kernelName;
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
} // namespace OCLRT
