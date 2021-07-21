/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <map>
#include <string_view>

namespace NEO {
std::map<std::string_view, std::vector<AILEnumeration>> applicationMap = {};

AILConfiguration *ailConfigurationTable[IGFX_MAX_PRODUCT] = {};

AILConfiguration *AILConfiguration::get(PRODUCT_FAMILY productFamily) {
    return ailConfigurationTable[productFamily];
}

void AILConfiguration::apply(RuntimeCapabilityTable &runtimeCapabilityTable) {
    applyExt(runtimeCapabilityTable);
}

} // namespace NEO