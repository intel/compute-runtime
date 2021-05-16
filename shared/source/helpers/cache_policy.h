/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

namespace CacheSettings {
constexpr uint32_t unknownMocs = GMM_RESOURCE_USAGE_UNKNOWN;
} // namespace CacheSettings

namespace NEO {
class GraphicsAllocation;
bool isL3Capable(void *ptr, size_t size);
bool isL3Capable(const GraphicsAllocation &graphicsAllocation);
} // namespace NEO
