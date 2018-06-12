/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "test.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/memory_manager.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/fixtures/hello_world_kernel_fixture.h"
#include "gen_cmd_parse.h"
#include "unit_tests/fixtures/buffer_fixture.h"

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

        pDestMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pDestMemory);
        pSrcMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pSrcMemory);

        pKernel->setArgSvm(0, sizeUserMemory, pSrcMemory);
        pKernel->setArgSvm(1, sizeUserMemory, pDestMemory);

        BufferDefaults::context = new MockContext(pDevice);
    }

    virtual void TearDown() {
        pCmdQ->flush();

        alignedFree(pSrcMemory);
        alignedFree(pDestMemory);

        KernelFixture::TearDown();
        IndirectHeapFixture::TearDown();
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        delete BufferDefaults::context;
        DeviceFixture::TearDown();
    }

    void *pSrcMemory;
    void *pDestMemory;
    size_t sizeUserMemory;
    const char *kernelFilename;
    const char *kernelName;

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
