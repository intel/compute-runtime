/*
 * Copyright (c) 2018, Intel Corporation
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

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/gen_common/test.h"

#include <cstdint>

namespace OCLRT {
class MockContext;
class MockKernel;
class MockProgram;
class Image;
struct KernelInfo;
} // namespace OCLRT

namespace iOpenCL {
struct SKernelBinaryHeaderCommon;
}

class KernelImageArgTest : public Test<OCLRT::DeviceFixture> {
  public:
    KernelImageArgTest() {
    }

    ~KernelImageArgTest() override;

  protected:
    void SetUp() override;

    void TearDown() override;

    cl_int retVal = 0;
    std::unique_ptr<iOpenCL::SKernelBinaryHeaderCommon> kernelHeader;
    std::unique_ptr<OCLRT::MockContext> context;
    std::unique_ptr<OCLRT::MockProgram> program;
    std::unique_ptr<OCLRT::KernelInfo> pKernelInfo;
    std::unique_ptr<OCLRT::MockKernel> pKernel;
    std::unique_ptr<OCLRT::Image> image;

    char surfaceStateHeap[0x80];
    uint32_t offsetNumMipLevelsImage0 = -1;
};
