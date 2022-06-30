/*
 * Copyright (C) 2018-2022 Intel Corporation
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
        debugVarSnapshot = DebugManager.flags;
        injectFcnSnapshot = DebugManager.injectFcn;
    }
    ~DebugManagerStateRestore() {
        DebugManager.flags = debugVarSnapshot;
        DebugManager.injectFcn = injectFcnSnapshot;
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) shrink(DebugManager.flags.variableName.getRef());
#include "shared/source/debug_settings/release_variables.inl"

#include "debug_variables.inl"
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
