/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_CRI> enableAILCRI;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapCRI = {};

template class AILConfigurationHw<IGFX_CRI>;

} // namespace NEO
