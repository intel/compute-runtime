/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <map>
namespace NEO {
static EnableAIL<IGFX_XE_HP_SDV> enableAILXEHPSDV;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapXEHPSDV = {};

template <>
inline void AILConfigurationHw<IGFX_XE_HP_SDV>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}
} // namespace NEO
