/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/file_io.h"
#include "unit_tests/aub_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "unit_tests/command_queue/command_enqueue_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/fixtures/run_kernel_fixture.h"
#include "unit_tests/global_environment.h"
#include "unit_tests/helpers/test_files.h"

namespace OCLRT {

////////////////////////////////////////////////////////////////////////////////
// Factory where all command stream traffic funnels to an AUB file
////////////////////////////////////////////////////////////////////////////////
struct AUBRunKernelFixtureFactory : public RunKernelFixtureFactory {
    typedef AUBCommandStreamFixture CommandStreamFixture;
};

////////////////////////////////////////////////////////////////////////////////
// RunKernelFixture
//      Instantiates a fixture based on the supplied fixture factory.
//      Performs proper initialization/shutdown of various elements in factory.
//      Used by most tests for integration testing with command queues.
////////////////////////////////////////////////////////////////////////////////
template <typename FixtureFactory>
class RunKernelFixture : public CommandEnqueueAUBFixture {
  public:
    RunKernelFixture() {
    }

    virtual void SetUp() {
        CommandEnqueueAUBFixture::SetUp();
    }

    virtual void TearDown() {
        CommandEnqueueAUBFixture::TearDown();
    }

  protected:
    Program *CreateProgramFromBinary(
        const std::string &binaryFileName) {
        cl_int retVal = CL_SUCCESS;

        EXPECT_EQ(true, fileExists(binaryFileName));

        void *pSource = nullptr;
        size_t sourceSize = loadDataFromFile(
            binaryFileName.c_str(),
            pSource);

        EXPECT_NE(0u, sourceSize);
        EXPECT_NE(nullptr, pSource);

        Program *pProgram = nullptr;
        const cl_device_id device = pDevice;

        pProgram = Program::create(
            context,
            1,
            &device,
            &sourceSize,
            (const unsigned char **)&pSource,
            nullptr,
            retVal);

        EXPECT_EQ(retVal, CL_SUCCESS);
        EXPECT_NE(pProgram, nullptr);

        deleteDataReadFromFile(pSource);

        return pProgram;
    }
};
} // namespace OCLRT
