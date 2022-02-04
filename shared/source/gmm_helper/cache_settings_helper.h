/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

namespace NEO {
enum class AllocationType;

namespace CacheSettingsHelper {
GMM_RESOURCE_USAGE_TYPE_ENUM getGmmUsageType(AllocationType allocationType, bool forceUncached);
}
} // namespace NEO