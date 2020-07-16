/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/driver/driver_handle.h"
#include <level_zero/ze_api.h>

#include "third_party/level_zero/ze_api_ext.h"

struct _ze_context_handle_t {
    virtual ~_ze_context_handle_t() = default;
};

namespace L0 {
struct DriverHandle;

struct Context : _ze_context_handle_t {
    virtual ~Context() = default;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t getStatus() = 0;
    virtual DriverHandle *getDriverHandle() = 0;

    static Context *fromHandle(ze_context_handle_t handle) { return static_cast<Context *>(handle); }
    inline ze_context_handle_t toHandle() { return this; }
};

} // namespace L0
