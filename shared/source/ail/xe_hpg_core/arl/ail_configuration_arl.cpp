/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_ARROWLAKE> enableAILARL;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapARL = {};

template class AILConfigurationHw<IGFX_ARROWLAKE>;

} // namespace NEO
