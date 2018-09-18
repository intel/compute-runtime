/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/options.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/mocks/mock_context.h"

namespace OCLRT {

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
        cl_device_id device = pPlatform->getDevice(0);
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();

        CreateProgramWithSource(
            pContext,
            &device,
            SourceFileName);
    }

    virtual void TearDown() {
        deleteDataReadFromFile(knownSource);
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
        delete kbHelper;
        CompilerInterface::shutdown();
    }

    KernelBinaryHelper *kbHelper;
    const char *SourceFileName;
    const char *BinaryFileName;
    const char *KernelName;
    cl_int retVal;
};
} // namespace OCLRT
