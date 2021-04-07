/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

struct _zet_debug_session_handle_t {};

namespace L0 {

struct Device;

struct DebugSession : _zet_debug_session_handle_t {
    virtual ~DebugSession() = default;
    DebugSession() = delete;

    static DebugSession *create(const zet_debug_config_t &config, Device *device, ze_result_t &result);

    static DebugSession *fromHandle(zet_debug_session_handle_t handle) { return static_cast<DebugSession *>(handle); }
    inline zet_debug_session_handle_t toHandle() { return this; }

    virtual bool closeConnection() = 0;
    virtual ze_result_t initialize() = 0;

    virtual ze_result_t readEvent(uint64_t timeout, zet_debug_event_t *event) = 0;

    Device *getConnectedDevice() { return connectedDevice; }

  protected:
    DebugSession(const zet_debug_config_t &config, Device *device) : connectedDevice(device){};
    Device *connectedDevice = nullptr;
};

} // namespace L0