/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/environment_variables.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

uint32_t getEnvironmentVariable(const char *name, char *outBuffer, uint32_t outBufferSize) {
    if (NEO::debugManager.registryReadAvailable() == false) {
        return 0;
    }
    return GetEnvironmentVariableA(name, outBuffer, outBufferSize);
}
