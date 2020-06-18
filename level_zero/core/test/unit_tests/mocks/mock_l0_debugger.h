/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/debugger/debugger_l0.h"

namespace L0 {
namespace ult {

class MockDebuggerL0 : public L0::DebuggerL0 {
  public:
    using L0::DebuggerL0::DebuggerL0;
    ~MockDebuggerL0() override = default;
};

} // namespace ult
} // namespace L0