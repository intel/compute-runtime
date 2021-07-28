/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <map>
namespace NEO {
static EnableAIL<IGFX_ALDERLAKE_P> enableAILADLP;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapADLP = {};

template <>
inline void AILConfigurationHw<IGFX_ALDERLAKE_P>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}
} // namespace NEO
