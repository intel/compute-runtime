/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"

#include <string>
#include <type_traits>

using namespace NEO;

class DebugManagerStateRestore {
  public:
    DebugManagerStateRestore() : debugVarSnapshot(debugManager.flags), injectFcnSnapshot(debugManager.injectFcn) {}
    ~DebugManagerStateRestore() {
        debugManager.flags = debugVarSnapshot;
        debugManager.injectFcn = injectFcnSnapshot;
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) shrink<dataType>(debugManager.flags.variableName);
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) \
    DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#include "debug_variables.inl"
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description)
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#define DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description) shrink<dataType>(debugManager.flags.variableName);
#define DECLARE_RAW_ENV_SCOPED_V(dataType, variableName, envVarName, defaultValue, description, ...) \
    DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description)
#define DECLARE_RAW_ENV_VARIABLE_OPT(enabled, dataType, variableName, envVarName, defaultValue, description) DECLARE_RAW_ENV_VARIABLE(dataType, variableName, envVarName, defaultValue, description)
#include "env_variables.inl"
#undef DECLARE_RAW_ENV_VARIABLE_OPT
#undef DECLARE_RAW_ENV_SCOPED_V
#undef DECLARE_RAW_ENV_VARIABLE
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE
    }
    DebugVariables debugVarSnapshot;
    void *injectFcnSnapshot = nullptr;

  protected:
    template <typename DataType, typename FlagType>
    static void shrink(FlagType &flag) {
        if constexpr (std::is_same_v<DataType, std::string>) {
            flag.getRef().shrink_to_fit();
        }
    }
};
