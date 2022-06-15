/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/ail/ail_configuration_base.inl"

#include <map>

namespace NEO {
static EnableAIL<IGFX_DG1> enableAILDG1;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapDG1 = {};

template class AILConfigurationHw<IGFX_DG1>;

} // namespace NEO
