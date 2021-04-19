/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"

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
    DebugSessionMock(const zet_debug_config_t &config, L0::Device *device) : DebugSession(config, device), config(config){};
    bool closeConnection() override { return true; }
    ze_result_t initialize() override {
        if (config.pid == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    void startAsyncThread() override {
        asyncThreadStarted = true;
    }

    zet_debug_config_t config;
    bool asyncThreadStarted = false;
};

} // namespace ult
} // namespace L0
