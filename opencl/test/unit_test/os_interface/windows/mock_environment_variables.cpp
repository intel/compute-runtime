/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/mock_environment_variables.h"

#include "shared/source/os_interface/windows/environment_variables.h"

extern uint32_t (*getEnvironmentVariableMock)(const char *name, char *outBuffer, uint32_t outBufferSize) = nullptr;

uint32_t getEnvironmentVariable(const char *name, char *outBuffer, uint32_t outBufferSize) {
    if (getEnvironmentVariableMock == nullptr) {
        return 0;
    }
    return getEnvironmentVariableMock(name, outBuffer, outBufferSize);
}
