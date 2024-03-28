/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"

namespace NEO {
class ReleaseHelper;
void verifyAnyRemainingOpenclCFeatures(const ReleaseHelper *releaseHelper, const cl_name_version *&iterator);
}; // namespace NEO