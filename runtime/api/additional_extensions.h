/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "config.h"
#include <CL/cl.h>

namespace NEO {

void *CL_API_CALL getAdditionalExtensionFunctionAddress(const char *funcName);
}
