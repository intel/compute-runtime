/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <map>
namespace NEO {
static EnableAIL<IGFX_DG2> enableAILDG2;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapDG2 = {};

template <>
inline void AILConfigurationHw<IGFX_DG2>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}
} // namespace NEO
