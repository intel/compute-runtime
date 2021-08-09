/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <map>
namespace NEO {
static EnableAIL<IGFX_ALDERLAKE_S> enableAILADLS;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapADLS = {};

template <>
inline void AILConfigurationHw<IGFX_ALDERLAKE_S>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}
} // namespace NEO
