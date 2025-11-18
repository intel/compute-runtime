/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "config.h"

#include <stdint.h>

#ifdef BRANCH_TYPE
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
// clang-format off
#define BRANCH_HEADER_PATH STRINGIFY(BRANCH_TYPE/StateSaveAreaHeaderWrapper.h)
// clang-format on
#include BRANCH_HEADER_PATH
#else
#include "StateSaveAreaHeaderWrapper.h"
#endif