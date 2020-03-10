/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

uint32_t getEnvironmentVariable(const char *name, char *outBuffer, uint32_t outBufferSize);
