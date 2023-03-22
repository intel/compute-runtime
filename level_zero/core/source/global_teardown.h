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
static const std::string loaderLibraryFilename = "ze_loader";

void globalDriverTeardown();
ze_result_t setDriverTeardownHandleInLoader(std::string loaderLibraryName);
} // namespace L0