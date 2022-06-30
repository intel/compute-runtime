/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"

namespace NEO {
ApiSpecificConfig::ApiType apiTypeForUlts = ApiSpecificConfig::OCL;
bool ApiSpecificConfig::isStatelessCompressionSupported() {
    return ApiSpecificConfig::ApiType::OCL == ApiSpecificConfig::getApiType();
}
bool ApiSpecificConfig::getHeapConfiguration() {
    return DebugManager.flags.UseExternalAllocatorForSshAndDsh.get();
}
bool ApiSpecificConfig::getBindlessConfiguration() {
    if (DebugManager.flags.UseBindlessMode.get() != -1) {
        return DebugManager.flags.UseBindlessMode.get();
    } else {
        return false;
    }
}

ApiSpecificConfig::ApiType ApiSpecificConfig::getApiType() {
    return apiTypeForUlts;
}

uint64_t ApiSpecificConfig::getReducedMaxAllocSize(uint64_t maxAllocSize) {
    return maxAllocSize / 2;
}

std::string ApiSpecificConfig::getName() {
    return "shared";
}
const char *ApiSpecificConfig::getRegistryPath() {
    return "";
}
} // namespace NEO
