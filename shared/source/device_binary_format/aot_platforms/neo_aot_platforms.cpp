/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "neo_aot_platforms.h"

namespace AOT {

const std::map<PRODUCT_CONFIG, std::vector<PRODUCT_CONFIG>> &getInvertedCompatibilityMapping() {
    static const std::map<PRODUCT_CONFIG, std::vector<PRODUCT_CONFIG>> invertedMapping = []() {
        std::map<PRODUCT_CONFIG, std::vector<PRODUCT_CONFIG>> inverted;

        for (const auto &[targetConfig, compatibleConfigs] : compatibilityMapping) {
            for (const auto &compatConfig : compatibleConfigs) {
                inverted[compatConfig].push_back(targetConfig);
            }
        }

        return inverted;
    }();

    return invertedMapping;
}

} // namespace AOT