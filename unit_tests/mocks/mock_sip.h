/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/built_ins/sip.h"

#include <memory>
#include <vector>

namespace NEO {
class MemoryAllocation;

class MockSipKernel : public SipKernel {
  public:
    using SipKernel::program;
    using SipKernel::type;

    MockSipKernel(SipKernelType type, Program *sipProgram);
    MockSipKernel();
    ~MockSipKernel();

    static std::vector<char> dummyBinaryForSip;
    static std::vector<char> getDummyGenBinary();
    static std::vector<char> getBinary();
    static void initDummyBinary();
    static void shutDown();

    GraphicsAllocation *getSipAllocation() const override;

    std::unique_ptr<MemoryAllocation> mockSipMemoryAllocation;
};
} // namespace NEO