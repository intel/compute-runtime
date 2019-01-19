/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>

extern uint32_t (*getEnvironmentVariableMock)(const char *name, char *outBuffer, uint32_t outBufferSize);
