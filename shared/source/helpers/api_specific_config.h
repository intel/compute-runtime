/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>

#pragma once
namespace NEO {
struct ApiSpecificConfig {
    enum ApiType { OCL,
                   L0 };
    static bool isStatelessCompressionSupported();
    static bool getHeapConfiguration();
    static bool getBindlessConfiguration();
    static ApiType getApiType();
    static const char *getAubPrefixForSpecificApi();
    static uint64_t getReducedMaxAllocSize(uint64_t maxAllocSize);
};
} // namespace NEO
