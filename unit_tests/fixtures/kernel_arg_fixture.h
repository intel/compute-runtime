/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

#include <cstdint>

namespace NEO {
class MockContext;
class MockKernel;
class MockProgram;
class Image;
struct KernelInfo;
} // namespace NEO

namespace iOpenCL {
struct SKernelBinaryHeaderCommon;
}

class KernelImageArgTest : public Test<NEO::DeviceFixture> {
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
    std::unique_ptr<NEO::KernelInfo> pKernelInfo;
    std::unique_ptr<NEO::MockKernel> pKernel;
    std::unique_ptr<NEO::Image> image;

    char surfaceStateHeap[0x80];
    uint32_t offsetNumMipLevelsImage0 = -1;
};
