/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/environment_variables.h"

#include "core/debug_settings/debug_settings_manager.h"

#include <Windows.h>

uint32_t getEnvironmentVariable(const char *name, char *outBuffer, uint32_t outBufferSize) {
    if (NEO::DebugManager.registryReadAvailable() == false) {
        return 0;
    }
    return GetEnvironmentVariableA(name, outBuffer, outBufferSize);
}
