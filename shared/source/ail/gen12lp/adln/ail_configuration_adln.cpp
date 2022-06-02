/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <map>
namespace NEO {
static EnableAIL<IGFX_ALDERLAKE_N> enableAILADLN;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapADLN = {};

template <>
inline void AILConfigurationHw<IGFX_ALDERLAKE_N>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}
} // namespace NEO
