/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include <cstdint>

namespace NEO {
class MockContext;
class MockKernel;
class MultiDeviceKernel;
class MockProgram;
class Image;
class MockKernelInfo;
struct KernelInfo;
} // namespace NEO

namespace iOpenCL {
struct SKernelBinaryHeaderCommon;
}

class KernelImageArgTest : public Test<NEO::ClDeviceFixture> {
  public:
    KernelImageArgTest() {
    }

    ~KernelImageArgTest() override;

  protected:
    void SetUp() override;

    void TearDown() override;

    cl_int retVal = 0;
    std::unique_ptr<iOpenCL::SKernelBinaryHeaderCommon> kernelHeader;
    std::unique_ptr<NEO::MockContext> context;
    std::unique_ptr<NEO::MockProgram> program;
    std::unique_ptr<NEO::MockKernelInfo> pKernelInfo;
    std::unique_ptr<NEO::MultiDeviceKernel> pMultiDeviceKernel;
    NEO::MockKernel *pKernel;
    std::unique_ptr<NEO::Image> image;

    char surfaceStateHeap[0x80];
    uint32_t offsetNumMipLevelsImage0 = 0x40;
};
