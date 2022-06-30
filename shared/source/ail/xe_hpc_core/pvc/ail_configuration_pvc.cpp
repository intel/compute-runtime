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
static EnableAIL<IGFX_PVC> enableAILPVC;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapPVC = {};

template class AILConfigurationHw<IGFX_PVC>;

} // namespace NEO
