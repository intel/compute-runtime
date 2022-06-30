/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/environment_variables.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include <windows.h>

uint32_t getEnvironmentVariable(const char *name, char *outBuffer, uint32_t outBufferSize) {
    if (NEO::DebugManager.registryReadAvailable() == false) {
        return 0;
    }
    return GetEnvironmentVariableA(name, outBuffer, outBufferSize);
}
