/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debugger/debugger.h"

namespace L0 {
class DebuggerL0 : public NEO::Debugger {
  public:
    bool isDebuggerActive() override;
    ~DebuggerL0() override = default;
};
} // namespace L0