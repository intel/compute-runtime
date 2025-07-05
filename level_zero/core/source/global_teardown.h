/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/loader/ze_loader.h>
#include <level_zero/ze_api.h>

namespace L0 {
extern decltype(&zelLoaderTranslateHandle) loaderTranslateHandleFunc;
extern decltype(&zelSetDriverTeardown) setDriverTeardownFunc;

void globalDriverTeardown();
void globalDriverSetup();
} // namespace L0
