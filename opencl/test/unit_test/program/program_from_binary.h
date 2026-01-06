/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
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
        ProgramFromBinaryFixture::setUp();
    }
    void setUp() {
        ClDeviceFixture::setUp();
        zebinPtr = std::make_unique<MockZebinWrapper<>>(pClDevice->getHardwareInfo());
        this->kernelName = zebinPtr->kernelName;
        cl_device_id device = pClDevice;
        ContextFixture::setUp(1, &device);
        ProgramFixture::setUp();

        createProgramFromBinary(pContext, pContext->getDevices(), zebinPtr->binaries.data(), zebinPtr->binarySizes.data());
        knownSourceSize = zebinPtr->binarySizes[0];
        knownSource = std::make_unique<char[]>(knownSourceSize);
        std::copy(zebinPtr->binaries[0], zebinPtr->binaries[0] + knownSourceSize, knownSource.get());
    }

    void TearDown() override {
        knownSource.reset();
        zebinPtr.reset();
        ProgramFixture::tearDown();
        ContextFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    std::unique_ptr<MockZebinWrapper<>> zebinPtr;
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
