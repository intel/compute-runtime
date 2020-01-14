/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/command_stream_receiver.h"
#include "unit_tests/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/simple_arg_fixture.h"
#include "unit_tests/fixtures/simple_arg_kernel_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"

namespace NEO {

////////////////////////////////////////////////////////////////////////////////
// Factory where all command stream traffic funnels to an AUB file
////////////////////////////////////////////////////////////////////////////////
struct AUBSimpleArgFixtureFactory : public SimpleArgFixtureFactory,
                                    public IndirectHeapFixture {
    typedef AUBCommandStreamFixture CommandStreamFixture;
};

////////////////////////////////////////////////////////////////////////////////
// SimpleArgTest
//      Instantiates a fixture based on the supplied fixture factory.
//      Performs proper initialization/shutdown of various elements in factory.
//      Used by most tests for integration testing with command queues.
////////////////////////////////////////////////////////////////////////////////
template <typename FixtureFactory>
struct SimpleArgFixture : public FixtureFactory::IndirectHeapFixture,
                          public FixtureFactory::CommandStreamFixture,
                          public FixtureFactory::CommandQueueFixture,
                          public FixtureFactory::KernelFixture,
                          public DeviceFixture {
    typedef typename FixtureFactory::IndirectHeapFixture IndirectHeapFixture;
    typedef typename FixtureFactory::CommandStreamFixture CommandStreamFixture;
    typedef typename FixtureFactory::CommandQueueFixture CommandQueueFixture;
    typedef typename FixtureFactory::KernelFixture KernelFixture;

    using AUBCommandStreamFixture::SetUp;
    using CommandQueueFixture::pCmdQ;
    using CommandStreamFixture::pCS;
    using IndirectHeapFixture::SetUp;
    using KernelFixture::pKernel;
    using KernelFixture::SetUp;

    SimpleArgFixture()
        : pDestMemory(nullptr), sizeUserMemory(128 * sizeof(float)) {
    }

  public:
    virtual void SetUp() {
        DeviceFixture::SetUp();
        ASSERT_NE(nullptr, pClDevice);
        CommandQueueFixture::SetUp(pClDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);
        CommandStreamFixture::SetUp(pCmdQ);
        ASSERT_NE(nullptr, pCS);
        IndirectHeapFixture::SetUp(pCmdQ);
        KernelFixture::SetUp(pClDevice);
        ASSERT_NE(nullptr, pKernel);

        argVal = static_cast<int>(0x22222222);
        pDestMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pDestMemory);

        pExpectedMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pExpectedMemory);

        // Initialize user memory to known values
        memset(pDestMemory, 0x11, sizeUserMemory);
        memset(pExpectedMemory, 0x22, sizeUserMemory);

        pKernel->setArg(0, sizeof(int), &argVal);
        pKernel->setArgSvm(1, sizeUserMemory, pDestMemory, nullptr, 0u);

        outBuffer = AUBCommandStreamFixture::createResidentAllocationAndStoreItInCsr(pDestMemory, sizeUserMemory);
        ASSERT_NE(nullptr, outBuffer);
        outBuffer->setAllocationType(GraphicsAllocation::AllocationType::BUFFER);
        outBuffer->setMemObjectsAllocationWithWritableFlags(true);
    }

    virtual void TearDown() {
        if (pExpectedMemory) {
            alignedFree(pExpectedMemory);
            pExpectedMemory = nullptr;
        }
        if (pDestMemory) {
            alignedFree(pDestMemory);
            pDestMemory = nullptr;
        }

        KernelFixture::TearDown();
        IndirectHeapFixture::TearDown();
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    int argVal;
    void *pDestMemory;
    void *pExpectedMemory;
    size_t sizeUserMemory;
    GraphicsAllocation *outBuffer;
};
} // namespace NEO
