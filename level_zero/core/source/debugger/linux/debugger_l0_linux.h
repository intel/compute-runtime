/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/debugger/debugger_l0.h"

namespace L0 {
class DebuggerL0Linux : public DebuggerL0 {
  public:
    DebuggerL0Linux(NEO::Device *device) : DebuggerL0(device) {
    }
    ~DebuggerL0Linux() override = default;
};
} // namespace L0