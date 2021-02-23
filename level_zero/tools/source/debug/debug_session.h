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

    static DebugSession *create(const zet_debug_config_t &config, Device *device);

    static DebugSession *fromHandle(zet_debug_session_handle_t handle) { return static_cast<DebugSession *>(handle); }
    inline zet_debug_session_handle_t toHandle() { return this; }

  protected:
    DebugSession(const zet_debug_config_t &config, Device *device){};
};

} // namespace L0