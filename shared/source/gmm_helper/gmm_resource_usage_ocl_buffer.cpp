/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_resource_usage_ocl_buffer.h"

#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/gmm_helper/gmm_resource_usage_type.h"

namespace NEO {
constexpr GmmResourceUsageType gmmResourceUsageOclBuffer = GMM_RESOURCE_USAGE_OCL_BUFFER;

static_assert(static_cast<GMM_RESOURCE_USAGE_TYPE_ENUM>(gmmResourceUsageOclBuffer) == GMM_RESOURCE_USAGE_OCL_BUFFER);
} // namespace NEO
