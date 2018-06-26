/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
