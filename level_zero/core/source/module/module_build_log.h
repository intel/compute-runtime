/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"

#include <string>

struct _ze_module_build_log_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_MODULE_BUILD_LOG> {};
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

inline void append(ModuleBuildLog &dst, ModuleBuildLog &src) {
    size_t srcLen = 0U;
    src.getString(&srcLen, nullptr);
    if (srcLen == 0) {
        return;
    }
    StackVec<char, 256> srcStr;
    srcStr.resize(srcLen);
    src.getString(&srcLen, srcStr.data());
    dst.appendString(srcStr.data(), srcLen);
}

inline std::string read(ModuleBuildLog &src) {
    size_t srcLen = 0U;
    src.getString(&srcLen, nullptr);
    if (srcLen == 0) {
        return "";
    }
    StackVec<char, 256> srcStr;
    srcStr.resize(srcLen);
    src.getString(&srcLen, srcStr.data());
    std::string ret;
    ret.assign(srcStr.data(), srcLen);
    return ret;
}

static_assert(NEO::NonCopyableAndNonMovable<ModuleBuildLog>);

} // namespace L0
