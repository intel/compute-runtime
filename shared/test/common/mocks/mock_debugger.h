/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger.h"

namespace NEO {
class CommandContainer;

class MockDebugger : public Debugger {
  public:
    MockDebugger() = default;
    ~MockDebugger() = default;
    void captureStateBaseAddress(CommandContainer &container, SbaAddresses sba){};
};
} // namespace NEO
