/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/api/additional_extensions.h"

namespace NEO {
void *CL_API_CALL getAdditionalExtensionFunctionAddress(const char *funcName) {
    return nullptr;
}

bool doNotReportClPlatform() {
    return false;
}

} // namespace NEO
