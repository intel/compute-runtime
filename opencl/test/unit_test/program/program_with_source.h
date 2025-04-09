/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/helpers/file_io.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"

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
        USE_REAL_FILE_SYSTEM();
        sourceFileName = "CopyBuffer_simd16.cl";
        binaryFileName = "CopyBuffer_simd16";
        kernelName = "CopyBuffer";
        kbHelper = new KernelBinaryHelper(binaryFileName);

        PlatformFixture::setUp();
        cl_device_id device = pPlatform->getClDevice(0);
        rootDeviceIndex = pPlatform->getClDevice(0)->getRootDeviceIndex();
        ContextFixture::setUp(1, &device);
        ProgramFixture::setUp();

        createProgramWithSource(
            pContext,
            sourceFileName);
    }

    void TearDown() override {
        knownSource.reset();
        ProgramFixture::tearDown();
        ContextFixture::tearDown();
        PlatformFixture::tearDown();
        delete kbHelper;
    }

    KernelBinaryHelper *kbHelper = nullptr;
    const char *sourceFileName = nullptr;
    const char *binaryFileName = nullptr;
    const char *kernelName = nullptr;
    cl_int retVal = CL_SUCCESS;
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
};
} // namespace NEO
