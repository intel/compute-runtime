/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

struct _ze_module_build_log_handle_t : BaseHandle {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_module_build_log_handle_t>);

namespace L0 {

struct Module;

struct ModuleBuildLog : _ze_module_build_log_handle_t, NEO::NonCopyableAndNonMovableClass {
    static ModuleBuildLog *create();
    virtual ~ModuleBuildLog() = default;

    virtual ze_result_t destroy() = 0;
    virtual ze_result_t getString(size_t *pSize, char *pBuildLog) = 0;
    virtual void appendString(const char *pBuildLog, size_t size) = 0;

    ModuleBuildLog() = default;
    ModuleBuildLog(const ModuleBuildLog &) = delete;
    ModuleBuildLog(ModuleBuildLog &&) = delete;
    ModuleBuildLog &operator=(const ModuleBuildLog &) = delete;
    ModuleBuildLog &operator=(ModuleBuildLog &&) = delete;

    static ModuleBuildLog *fromHandle(ze_module_build_log_handle_t handle) {
        return static_cast<ModuleBuildLog *>(handle);
    }

    inline ze_module_build_log_handle_t toHandle() { return this; }
};

static_assert(NEO::NonCopyableAndNonMovable<ModuleBuildLog>);

} // namespace L0
