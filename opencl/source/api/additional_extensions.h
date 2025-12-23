/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "config.h"
#include <CL/cl.h>

namespace NEO {

void *CL_API_CALL getAdditionalExtensionFunctionAddress(const char *funcName);

bool doNotReportClPlatform();

} // namespace NEO
