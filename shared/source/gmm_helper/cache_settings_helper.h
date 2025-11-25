/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_resource_usage_type.h"

#include <cstddef>
#include <cstdint>

namespace CacheSettings {
inline constexpr uint32_t unknownMocs = -1;
} // namespace CacheSettings

namespace NEO {

enum class AllocationType;
struct HardwareInfo;
class ProductHelper;
struct RootDeviceEnvironment;

struct CacheSettingsHelper {
    static GmmResourceUsageType getGmmUsageType(AllocationType allocationType, bool forceUncached, const ProductHelper &productHelper, const HardwareInfo *hwInfo);
    static GmmResourceUsageType getGmmUsageTypeForUserPtr(bool isCacheFlushRequired, const void *userPtr, size_t size, const ProductHelper &productHelper);

    static bool isUncachedType(GmmResourceUsageType gmmResourceUsageType);

    static bool preferNoCpuAccess(GmmResourceUsageType gmmResourceUsageType, const RootDeviceEnvironment &rootDeviceEnvironment);

  protected:
    static GmmResourceUsageType getDefaultUsageTypeWithCachingEnabled(AllocationType allocationType, const ProductHelper &productHelper, const HardwareInfo *hwInfo);
    static GmmResourceUsageType getDefaultUsageTypeWithCachingDisabled(AllocationType allocationType, const ProductHelper &productHelper);
};
} // namespace NEO
