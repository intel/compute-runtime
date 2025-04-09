/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/helpers/file_io.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include <string>

namespace NEO {

////////////////////////////////////////////////////////////////////////////////
// ProgramFromBinaryTest Test Fixture
//      Used to test the Program class
////////////////////////////////////////////////////////////////////////////////
struct ProgramFromBinaryFixture : public ClDeviceFixture,
                                  public ContextFixture,
                                  public ProgramFixture,
                                  public testing::Test {

    using ContextFixture::setUp;

    void SetUp() override {
        ProgramFromBinaryFixture::setUp("CopyBuffer_simd32", "CopyBuffer");
    }
    void setUp(const char *binaryFileName, const char *kernelName) {
        USE_REAL_FILE_SYSTEM();
        this->binaryFileName = binaryFileName;
        this->kernelName = kernelName;
        ClDeviceFixture::setUp();

        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        ProgramFixture::setUp();

        if (options.size())
            createProgramFromBinary(pContext, pContext->getDevices(), binaryFileName, options);
        else
            createProgramFromBinary(pContext, pContext->getDevices(), binaryFileName);
    }

    void TearDown() override {
        knownSource.reset();
        ProgramFixture::tearDown();
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    void setOptions(std::string &optionsIn) {
        options = optionsIn;
    }

    const char *binaryFileName = nullptr;
    const char *kernelName = nullptr;
    cl_int retVal = CL_SUCCESS;
    std::string options;
};

////////////////////////////////////////////////////////////////////////////////
// ProgramSimpleFixture Test Fixture
//      Used to test the Program class, but not using parameters
////////////////////////////////////////////////////////////////////////////////
class ProgramSimpleFixture : public ClDeviceFixture,
                             public ContextFixture,
                             public ProgramFixture {
    using ContextFixture::setUp;

  public:
    void setUp() {
        USE_REAL_FILE_SYSTEM();
        ClDeviceFixture::setUp();

        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        ProgramFixture::setUp();
    }

    void tearDown() {
        knownSource.reset();
        ProgramFixture::tearDown();
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

  protected:
    cl_int retVal = CL_SUCCESS;
};
} // namespace NEO
