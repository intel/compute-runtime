/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

namespace L0 {
namespace ult {

class MockBuiltins : public NEO::BuiltIns {
  public:
    MockBuiltins() : BuiltIns() {
        allocation.reset(new NEO::MockGraphicsAllocation());
    }
    const NEO::SipKernel &getSipKernel(NEO::SipKernelType type, NEO::Device &device) override;
    std::unique_ptr<NEO::SipKernel> sipKernel;
    std::unique_ptr<NEO::MockGraphicsAllocation> allocation;
    std::vector<char> stateSaveAreaHeader{'s', 's', 'a', 'h'};
};
} // namespace ult
} // namespace L0
