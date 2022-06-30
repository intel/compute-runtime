/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"

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

    using ContextFixture::SetUp;

    void SetUp() override {
        ProgramFromBinaryFixture::SetUp("CopyBuffer_simd32", "CopyBuffer");
    }
    void SetUp(const char *binaryFileName, const char *kernelName) {
        this->binaryFileName = binaryFileName;
        this->kernelName = kernelName;
        ClDeviceFixture::SetUp();

        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();

        if (options.size())
            CreateProgramFromBinary(pContext, pContext->getDevices(), binaryFileName, options);
        else
            CreateProgramFromBinary(pContext, pContext->getDevices(), binaryFileName);
    }

    void TearDown() override {
        knownSource.reset();
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        ClDeviceFixture::TearDown();
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
    using ContextFixture::SetUp;

  public:
    void SetUp() override {
        ClDeviceFixture::SetUp();

        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();
    }

    void TearDown() override {
        knownSource.reset();
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        ClDeviceFixture::TearDown();
    }

  protected:
    cl_int retVal = CL_SUCCESS;
};
} // namespace NEO
