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
#include "runtime/built_ins/built_ins.h"
#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/program_fixture.h"
#include "unit_tests/mocks/mock_context.h"

#include <string>

namespace OCLRT {

////////////////////////////////////////////////////////////////////////////////
// ProgramFromBinaryTest Test Fixture
//      Used to test the Program class
////////////////////////////////////////////////////////////////////////////////
class ProgramFromBinaryTest : public DeviceFixture,
                              public ContextFixture,
                              public ProgramFixture,
                              public testing::TestWithParam<std::tuple<const char *, const char *>> {

    using ContextFixture::SetUp;

  protected:
    ProgramFromBinaryTest() : BinaryFileName(nullptr),
                              KernelName(nullptr),
                              retVal(CL_SUCCESS) {
    }

    void SetUp() override {
        std::tie(BinaryFileName, KernelName) = GetParam();

        DeviceFixture::SetUp();

        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();

        if (options.size())
            CreateProgramFromBinary<Program>(pContext, &device, BinaryFileName, options);
        else
            CreateProgramFromBinary<Program>(pContext, &device, BinaryFileName);
    }

    void TearDown() override {
        deleteDataReadFromFile(knownSource);
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    void setOptions(std::string &optionsIn) {
        options = optionsIn;
    }

    const char *BinaryFileName;
    const char *KernelName;
    cl_int retVal;
    std::string options;
};

////////////////////////////////////////////////////////////////////////////////
// ProgramSimpleFixture Test Fixture
//      Used to test the Program class, but not using parameters
////////////////////////////////////////////////////////////////////////////////
class ProgramSimpleFixture : public DeviceFixture,
                             public ContextFixture,
                             public ProgramFixture {
    using ContextFixture::SetUp;

  public:
    ProgramSimpleFixture() : retVal(CL_SUCCESS) {
    }

    void SetUp() override {
        DeviceFixture::SetUp();

        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();
    }

    void TearDown() override {
        deleteDataReadFromFile(knownSource);
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

  protected:
    cl_int retVal;
};
} // namespace OCLRT
