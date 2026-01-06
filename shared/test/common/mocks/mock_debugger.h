/*
 * Copyright (C) 2021-2025 Intel Corporation
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
    void captureStateBaseAddress(NEO::LinearStream &cmdStream, SbaAddresses sba, bool useFirstLevelBB) override{};
    size_t getSbaTrackingCommandsSize(size_t trackedAddressCount) override {
        return 0;
    }
    bool getSingleAddressSpaceSbaTracking() const override {
        return singleAddressSpaceSbaTracking;
    }
    bool singleAddressSpaceSbaTracking = false;
};
} // namespace NEO
