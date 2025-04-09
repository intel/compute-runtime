/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

namespace CacheSettings {
inline constexpr uint32_t unknownMocs = -1;
} // namespace CacheSettings

namespace NEO {
enum class AllocationType;
struct HardwareInfo;
class ProductHelper;
struct RootDeviceEnvironment;

struct CacheSettingsHelper {
    static GMM_RESOURCE_USAGE_TYPE_ENUM getGmmUsageType(AllocationType allocationType, bool forceUncached, const ProductHelper &productHelper, const HardwareInfo *hwInfo);
    static GMM_RESOURCE_USAGE_TYPE_ENUM getGmmUsageTypeForUserPtr(bool isCacheFlushRequired, const void *userPtr, size_t size, const ProductHelper &productHelper);

    static constexpr bool isUncachedType(GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsageType) {
        return ((gmmResourceUsageType == GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC) ||
                (gmmResourceUsageType == GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED) ||
                (gmmResourceUsageType == GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) ||
                (gmmResourceUsageType == GMM_RESOURCE_USAGE_SURFACE_UNCACHED));
    }

    static bool preferNoCpuAccess(GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsageType, const RootDeviceEnvironment &rootDeviceEnvironment);

  protected:
    static GMM_RESOURCE_USAGE_TYPE_ENUM getDefaultUsageTypeWithCachingEnabled(AllocationType allocationType, const ProductHelper &productHelper, const HardwareInfo *hwInfo);
    static GMM_RESOURCE_USAGE_TYPE_ENUM getDefaultUsageTypeWithCachingDisabled(AllocationType allocationType, const ProductHelper &productHelper);
};
} // namespace NEO
