/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/cache_settings_helper.h"

namespace NEO {
class MockCacheSettingsHelper : public CacheSettingsHelper {
  public:
    using CacheSettingsHelper::isCpuCachingOfDeviceBuffersAllowed;
};
} // namespace NEO
