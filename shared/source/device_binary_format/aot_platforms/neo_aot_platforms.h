/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "platforms.h"

namespace AOT {
consteval PRODUCT_CONFIG getConfixMaxPlatform() {
    return CONFIG_MAX_PLATFORM;
}

inline const auto &getCompatibilityMapping() {
    return compatibilityMapping;
}

inline const auto &getRtlIdAcronyms() {
    return rtlIdAcronyms;
}

const std::map<PRODUCT_CONFIG, std::vector<PRODUCT_CONFIG>> &getInvertedCompatibilityMapping();
} // namespace AOT
