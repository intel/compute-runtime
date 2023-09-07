/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>

#include <string>

namespace L0 {
using zelSetDriverTeardown_fn = ze_result_t (*)();

void globalDriverTeardown();
ze_result_t setDriverTeardownHandleInLoader(const char *);
} // namespace L0
