/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/debug/debug_session.h"

namespace L0 {
namespace ult {

class OsInterfaceWithDebugAttach : public NEO::OSInterface {
  public:
    OsInterfaceWithDebugAttach() : OSInterface() {}
    bool isDebugAttachAvailable() const override {
        return debugAttachAvailable;
    }

    bool debugAttachAvailable = true;
};

struct DebugSessionMock : public L0::DebugSession {
    DebugSessionMock(const zet_debug_config_t &config, L0::Device *device) : DebugSession(config, device){};
    bool closeConnection() override { return true; }
};

} // namespace ult
} // namespace L0
