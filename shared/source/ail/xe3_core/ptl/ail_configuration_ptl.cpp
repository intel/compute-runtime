/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/ail/ail_ov_comp_wa.h"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_PTL> enableAILPTL;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapPTL = {};

template <>
void AILConfigurationHw<IGFX_PTL>::applyExt(HardwareInfo &hwInfo) {
    applyOpenVinoCompatibilityWaIfNeeded(hwInfo);
}

template class AILConfigurationHw<IGFX_PTL>;

} // namespace NEO
