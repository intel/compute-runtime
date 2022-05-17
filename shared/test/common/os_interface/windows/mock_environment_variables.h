/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

extern uint32_t (*getEnvironmentVariableMock)(const char *name, char *outBuffer, uint32_t outBufferSize);
