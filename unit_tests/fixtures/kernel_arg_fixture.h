/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
