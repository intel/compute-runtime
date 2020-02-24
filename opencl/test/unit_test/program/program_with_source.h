/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/compiler_interface.h"

#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/fixtures/program_fixture.h"
#include "opencl/test/unit_test/helpers/kernel_binary_helper.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

namespace NEO {

// ProgramFromSource Test Fixture
//      Used to test the Program class
////////////////////////////////////////////////////////////////////////////////
class ProgramFromSourceTest : public ContextFixture,
                              public PlatformFixture,
                              public ProgramFixture,
                              public testing::TestWithParam<std::tuple<const char *, const char *, const char *>> {

    using ContextFixture::SetUp;
    using PlatformFixture::SetUp;

  protected:
    ProgramFromSourceTest()
        : kbHelper(nullptr), SourceFileName(nullptr), KernelName(nullptr), retVal(CL_SUCCESS) {
    }

    virtual void SetUp() {
        std::tie(SourceFileName, BinaryFileName, KernelName) = GetParam();
        kbHelper = new KernelBinaryHelper(BinaryFileName);

        PlatformFixture::SetUp();
        cl_device_id device = pPlatform->getClDevice(0);
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();

        CreateProgramWithSource(
            pContext,
            &device,
            SourceFileName);
    }

    virtual void TearDown() {
        knownSource.reset();
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
        delete kbHelper;
    }

    KernelBinaryHelper *kbHelper;
    const char *SourceFileName;
    const char *BinaryFileName;
    const char *KernelName;
    cl_int retVal;
};
} // namespace NEO
