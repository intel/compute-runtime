/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/product_config_helper.h"

#include "platforms.h"

#include <algorithm>

void ProductConfigHelper::initialize() {
    for (auto &device : deviceAotInfo) {
        for (const auto &[acronym, value] : AOT::deviceAcronyms) {
            if (value == device.aotConfig.value) {
                device.deviceAcronyms.push_back(NEO::ConstStringRef(acronym));
            }
        }
    }
}

AOT::PRODUCT_CONFIG ProductConfigHelper::getProductConfigFromAcronym(const std::string &device) {
    auto it = std::find_if(AOT::deviceAcronyms.begin(), AOT::deviceAcronyms.end(), findMapAcronymWithoutDash(device));
    if (it == AOT::deviceAcronyms.end())
        return AOT::UNKNOWN_ISA;
    return it->second;
}
