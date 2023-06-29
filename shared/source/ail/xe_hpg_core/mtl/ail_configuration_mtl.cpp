/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"

#include <map>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_METEORLAKE> enableAILMTL;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapMTL = {
    {"Photoshop", {AILEnumeration::DISABLE_DEFAULT_CCS}},
    {"AfterFX (Beta)", {AILEnumeration::DISABLE_DEFAULT_CCS}},
    {"AfterFX", {AILEnumeration::DISABLE_DEFAULT_CCS}},
    {"Adobe Premiere Pro", {AILEnumeration::DISABLE_DEFAULT_CCS}},
};

template <>
inline void AILConfigurationHw<IGFX_METEORLAKE>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {

    auto search = applicationMapMTL.find(processName);

    if (search != applicationMapMTL.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::DISABLE_DEFAULT_CCS:
                runtimeCapabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
            default:
                break;
            }
        }
    }
}

template class AILConfigurationHw<IGFX_METEORLAKE>;

} // namespace NEO
