/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/mocks/mock_zebin_wrapper.h"

#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

// ProgramFromSource Test Fixture
//      Used to test the Program class
////////////////////////////////////////////////////////////////////////////////
class ProgramFromSourceTest : public ContextFixture,
                              public PlatformFixture,
                              public ProgramFixture,
                              public testing::TestWithParam<std::tuple<const char *, const char *, const char *>> {

    using ContextFixture::setUp;
    using PlatformFixture::setUp;

  protected:
    void SetUp() override {
        PlatformFixture::setUp();
        zebinPtr = std::make_unique<MockZebinWrapper<>>(pPlatform->getClDevice(0)->getHardwareInfo());

        cl_device_id device = pPlatform->getClDevice(0);
        rootDeviceIndex = pPlatform->getClDevice(0)->getRootDeviceIndex();
        ContextFixture::setUp(1, &device);
        ProgramFixture::setUp();

        createProgramWithSource(pContext);
    }

    void TearDown() override {
        knownSource.reset();
        zebinPtr.reset();
        ProgramFixture::tearDown();
        ContextFixture::tearDown();
        PlatformFixture::tearDown();
    }

    std::unique_ptr<MockZebinWrapper<>> zebinPtr;
    cl_int retVal = CL_SUCCESS;
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
};
} // namespace NEO