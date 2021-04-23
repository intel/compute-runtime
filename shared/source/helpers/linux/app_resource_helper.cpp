/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/app_resource_helper.h"

namespace NEO {

void AppResourceHelper::copyResourceTagStr(char *dst, GraphicsAllocation::AllocationType type, size_t size) {}
const char *AppResourceHelper::getResourceTagStr(GraphicsAllocation::AllocationType type) { return ""; }

} // namespace NEO
