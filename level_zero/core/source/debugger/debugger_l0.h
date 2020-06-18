/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <memory>

namespace NEO {
class Device;
}

namespace L0 {
class DebuggerL0 : public NEO::Debugger, NEO::NonCopyableOrMovableClass {
  public:
    static std::unique_ptr<Debugger> create(NEO::Device *device);
    bool isDebuggerActive() override;

    DebuggerL0(NEO::Device *device) : device(device) {
        isLegacyMode = false;
    }
    ~DebuggerL0() override = default;

  protected:
    NEO::Device *device = nullptr;
};
} // namespace L0