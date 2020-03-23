/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/built_ins.h"

namespace L0 {
namespace ult {

class MockBuiltins : public NEO::BuiltIns {
  public:
    const NEO::SipKernel &getSipKernel(NEO::SipKernelType type, NEO::Device &device) override;
};
} // namespace ult
} // namespace L0
