/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_METEORLAKE> enableAILMTL;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapMTL = {};

template class AILConfigurationHw<IGFX_METEORLAKE>;

} // namespace NEO
