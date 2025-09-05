/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"

using namespace NEO;

class DebugManagerStateRestore {
  public:
    DebugManagerStateRestore() {
        debugVarSnapshot = debugManager.flags;
        injectFcnSnapshot = debugManager.injectFcn;
    }
    ~DebugManagerStateRestore() {
        debugManager.flags = debugVarSnapshot;
        debugManager.injectFcn = injectFcnSnapshot;
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) shrink(debugManager.flags.variableName.getRef());
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE
    }
    DebugVariables debugVarSnapshot;
    void *injectFcnSnapshot = nullptr;

  protected:
    void shrink(std::string &flag) {
        flag.shrink_to_fit();
    }
    void shrink(int64_t &flag) {}
    void shrink(int32_t &flag) {}
    void shrink(bool &flag) {}
};
