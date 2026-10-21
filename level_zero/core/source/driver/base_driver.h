/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/helpers/api_handle_helper.h"

struct _ze_driver_handle_t : BaseHandle {};

namespace L0 {

struct BaseDriver : _ze_driver_handle_t {
    virtual ~BaseDriver() = default;
    virtual ze_result_t getExtensionFunctionAddress(const char *pFuncName, void **pfunc) = 0;
    static BaseDriver *fromHandle(ze_driver_handle_t handle) { return static_cast<BaseDriver *>(handle); }
};

} // namespace L0
