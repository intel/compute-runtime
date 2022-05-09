/*
 * Copyright (C) 2021-2022 Intel Corporation
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
    ~MockDebugger() override = default;
    void captureStateBaseAddress(NEO::LinearStream &cmdStream, SbaAddresses sba) override{};
    size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) override {
        return 0;
    }
};
} // namespace NEO
