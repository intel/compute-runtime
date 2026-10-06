/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"

#include <map>
#include <vector>

constexpr static auto gfxProduct = IGFX_ALDERLAKE_S;

#include "shared/source/ail/ail_configuration_microsecond_resolution_adjustment.inl"

namespace NEO {
static EnableAIL<gfxProduct> enableAILADLS;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapADLS = {};

template class AILConfigurationHw<gfxProduct>;

} // namespace NEO
