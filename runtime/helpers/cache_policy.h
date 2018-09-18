/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_lib.h"

namespace CacheSettings {
constexpr uint32_t l3CacheOn = GMM_RESOURCE_USAGE_OCL_BUFFER;
constexpr uint32_t l3CacheOff = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
constexpr uint32_t unknownMocs = GMM_RESOURCE_USAGE_UNKNOWN;
} // namespace CacheSettings

bool isL3Capable(void *ptr, size_t size);