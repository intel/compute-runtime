/*
 * Copyright (C) 2022 Intel Corporation
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
enum class AllocationType;
struct HardwareInfo;

struct CacheSettingsHelper {
    static GMM_RESOURCE_USAGE_TYPE_ENUM getGmmUsageType(AllocationType allocationType, bool forceUncached, const HardwareInfo &hwInfo);

    static constexpr bool isUncachedType(GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsageType) {
        return ((gmmResourceUsageType == GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC) ||
                (gmmResourceUsageType == GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED) ||
                (gmmResourceUsageType == GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    }

  protected:
    static GMM_RESOURCE_USAGE_TYPE_ENUM getDefaultUsageTypeWithCachingEnabled(AllocationType allocationType, const HardwareInfo &hwInfo);
    static GMM_RESOURCE_USAGE_TYPE_ENUM getDefaultUsageTypeWithCachingDisabled(AllocationType allocationType);
};
} // namespace NEO