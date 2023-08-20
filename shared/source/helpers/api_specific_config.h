/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"

#include <cstdint>
#include <string>
#include <vector>

namespace NEO {

struct ApiSpecificConfig {
    enum ApiType { OCL,
                   L0 };
    static bool isStatelessCompressionSupported();
    static bool getGlobalBindlessHeapConfiguration();
    static bool getBindlessMode();
    static bool isDeviceAllocationCacheEnabled();
    static bool isDynamicPostSyncAllocLayoutEnabled();
    static ApiType getApiType();
    static std::string getName();
    static uint64_t getReducedMaxAllocSize(uint64_t maxAllocSize);
    static const char *getRegistryPath();
    static void initPrefixes();
    static const std::vector<const char *> &getPrefixStrings();
    static const std::vector<DebugVarPrefix> &getPrefixTypes();
    static std::string getAubPrefixForSpecificApi() {
        return (getName() + "_");
    }
};
} // namespace NEO
