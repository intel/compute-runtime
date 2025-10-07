/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "platforms.h"

#include <algorithm>

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

inline std::vector<std::string> getCompatibilityFallbackProductAbbreviations(const std::string &requestedProductAbbreviation) {
    std::vector<std::string> result;
    PRODUCT_CONFIG requestedConfig = PRODUCT_CONFIG::UNKNOWN_ISA;

    std::string searchKey = requestedProductAbbreviation;

    for (const auto &acronymEntry : deviceAcronyms) {
        if (acronymEntry.first == searchKey || acronymEntry.first.find(searchKey + "-") == 0) {
            requestedConfig = acronymEntry.second;
            break;
        }
    }

    if (requestedConfig == PRODUCT_CONFIG::UNKNOWN_ISA) {
        return result;
    }

    for (const auto &compatibilityEntry : compatibilityMapping) {
        bool foundInThisEntry = false;
        for (const auto &compatibleConfig : compatibilityEntry.second) {
            if (compatibleConfig == requestedConfig) {
                foundInThisEntry = true;
                break;
            }
        }

        if (foundInThisEntry) {
            for (const auto &acronymEntry : deviceAcronyms) {
                if (acronymEntry.second == compatibilityEntry.first) {
                    std::string compatibleProductName = acronymEntry.first;
                    size_t dashPos = compatibleProductName.find('-');
                    if (dashPos != std::string::npos) {
                        compatibleProductName = compatibleProductName.substr(0, dashPos);
                    }

                    if (std::find(result.begin(), result.end(), compatibleProductName) == result.end()) {
                        result.push_back(compatibleProductName);
                    }
                    break;
                }
            }
        }
    }

    return result;
}
} // namespace AOT
