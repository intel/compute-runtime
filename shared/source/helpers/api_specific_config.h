/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/utilities/stackvec.h"

#include <cstdint>
#include <string>
#include <vector>

namespace NEO {
class Device;
class ReleaseHelper;
struct RootDeviceEnvironment;

struct ApiSpecificConfig {
    enum ApiType { OCL,
                   L0 };
    static bool isStatelessCompressionSupported();
    static bool getGlobalBindlessHeapConfiguration(const ReleaseHelper *releaseHelper);
    static bool getBindlessMode(const Device &device);
    static bool isDeviceAllocationCacheEnabled();
    static bool isHostAllocationCacheEnabled();
    static bool isDeviceUsmPoolingEnabled();
    static bool isHostUsmPoolingEnabled();
    static bool isGlobalStatelessEnabled(const RootDeviceEnvironment &rootDeviceEnvironment);
    static ApiType getApiType();
    static std::string getName();
    static uint64_t getReducedMaxAllocSize(uint64_t maxAllocSize);
    static const char *getRegistryPath();
    static void initPrefixes();
    static const StackVec<const char *, 4> &getPrefixStrings();
    static const StackVec<DebugVarPrefix, 4> &getPrefixTypes();
    static std::string getAubPrefixForSpecificApi() {
        return (getName() + "_");
    }
    static bool isSharedAllocPrefetchEnabled();
    static std::string compilerCacheDir();
    static std::string compilerCacheLocation();
    static std::string compilerCacheFileExtension();
    static int64_t compilerCacheDefaultEnabled();
    static bool isUpdateTagFromWaitEnabledForHeapless();
};
} // namespace NEO
