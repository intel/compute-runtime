/*
 * Copyright (C) 2020-2024 Intel Corporation
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
class ReleaseHelper;

struct ApiSpecificConfig {
    enum ApiType { OCL,
                   L0 };
    static bool isStatelessCompressionSupported();
    static bool getGlobalBindlessHeapConfiguration();
    static bool getBindlessMode(const ReleaseHelper *);
    static bool isDeviceAllocationCacheEnabled();
    static bool isHostAllocationCacheEnabled();
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
};
} // namespace NEO
