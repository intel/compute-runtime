/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <string>

namespace NEO {

template <PRODUCT_FAMILY Product>
void AILConfigurationHw<Product>::modifyKernelIfRequired(std::string &kernel) {
}

template <PRODUCT_FAMILY Product>
inline void AILConfigurationHw<Product>::forceFallbackToPatchtokensIfRequired(const std::string &kernelSources, bool &requiresFallback) {
}

template <PRODUCT_FAMILY Product>
inline void AILConfigurationHw<Product>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}

} // namespace NEO
