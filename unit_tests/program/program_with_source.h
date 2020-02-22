/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/compiler_interface/compiler_interface.h"

#include "fixtures/context_fixture.h"
#include "fixtures/memory_management_fixture.h"
#include "fixtures/platform_fixture.h"
#include "fixtures/program_fixture.h"
#include "helpers/kernel_binary_helper.h"
#include "mocks/mock_context.h"

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
