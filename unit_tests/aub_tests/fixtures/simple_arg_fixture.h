/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/command_stream_receiver.h"
#include "unit_tests/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/fixtures/simple_arg_fixture.h"
#include "unit_tests/fixtures/simple_arg_kernel_fixture.h"

namespace OCLRT {

////////////////////////////////////////////////////////////////////////////////
// Factory where all command stream traffic funnels to an AUB file
////////////////////////////////////////////////////////////////////////////////
struct AUBSimpleArgFixtureFactory : public SimpleArgFixtureFactory {
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
    using SimpleArgKernelFixture::SetUp;

    SimpleArgFixture()
        : pDestMemory(nullptr), sizeUserMemory(128 * sizeof(float)) {
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
        KernelFixture::SetUp(pDevice);
        ASSERT_NE(nullptr, pKernel);

        int argVal = (int)0x22222222;
        pDestMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pDestMemory);

        pExpectedMemory = alignedMalloc(sizeUserMemory, 4096);
        ASSERT_NE(nullptr, pExpectedMemory);

        // Initialize user memory to known values
        memset(pDestMemory, 0x11, sizeUserMemory);
        memset(pExpectedMemory, 0x22, sizeUserMemory);

        pKernel->setArg(0, sizeof(int), &argVal);
        pKernel->setArgSvm(1, sizeUserMemory, pDestMemory);

        auto &commandStreamReceiver = pDevice->getCommandStreamReceiver();
        commandStreamReceiver.createAllocationAndHandleResidency(pDestMemory, sizeUserMemory);
    }

    virtual void TearDown() {
        alignedFree(pExpectedMemory);
        alignedFree(pDestMemory);

        KernelFixture::TearDown();
        IndirectHeapFixture::TearDown();
        CommandStreamFixture::TearDown();
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    void *pDestMemory;
    void *pExpectedMemory;
    size_t sizeUserMemory;
};
} // namespace OCLRT
