/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_variables_helper.h"

#include "shared/source/os_interface/debug_env_reader.h"

namespace NEO {

bool isDebugKeysReadEnabled() {
    EnvironmentVariableReader envReader;
    return envReader.getSetting("NEOReadDebugKeys", false);
}

} // namespace NEO
