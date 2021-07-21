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

extern std::map<std::string_view, std::vector<AILEnumeration>> applicationMapHW;

template <PRODUCT_FAMILY Product>
void AILConfigurationHw<Product>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}
} // namespace NEO
