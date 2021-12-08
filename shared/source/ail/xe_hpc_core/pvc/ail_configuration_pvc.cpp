/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <map>
namespace NEO {
static EnableAIL<IGFX_PVC> enableAILPVC;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapPVC = {};

template <>
inline void AILConfigurationHw<IGFX_PVC>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}
} // namespace NEO
