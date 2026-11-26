/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/inject_debug_keys.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debug_settings/definitions/translate_debug_settings.h"
#include "shared/source/os_interface/debug_env_reader.h"

namespace NEO {

void injectDebugKeys(DebugVariables &flags, const std::string &envNamePrefix) {
    EnvironmentVariableReader reader;

#define INJECT_DEBUG_KEY(variableName) flags.variableName.set(reader.getSetting((envNamePrefix + #variableName).c_str(), flags.variableName.get()));
#define DECLARE_DEBUG_VARIABLE(dataType, variableName, defaultValue, description) INJECT_DEBUG_KEY(variableName)
#define DECLARE_DEBUG_SCOPED_V(dataType, variableName, defaultValue, description, ...) INJECT_DEBUG_KEY(variableName)
#define DECLARE_DEBUG_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) INJECT_DEBUG_KEY(variableName)
#include "debug_variables.inl"
#undef DECLARE_DEBUG_VARIABLE_OPT
#undef DECLARE_DEBUG_SCOPED_V
#undef DECLARE_DEBUG_VARIABLE
#define DECLARE_RELEASE_VARIABLE(dataType, variableName, defaultValue, description) INJECT_DEBUG_KEY(variableName)
#define DECLARE_RELEASE_VARIABLE_OPT(enabled, dataType, variableName, defaultValue, description) INJECT_DEBUG_KEY(variableName)
#include "release_variables.inl"
#undef DECLARE_RELEASE_VARIABLE_OPT
#undef DECLARE_RELEASE_VARIABLE
#undef INJECT_DEBUG_KEY

    translateDebugSettings(flags);
}

} // namespace NEO
