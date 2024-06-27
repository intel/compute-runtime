/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/helpers/hw_info.h"

#include "aubstream/engine_node.h"

#include <algorithm>
#include <map>
#include <vector>

namespace NEO {

extern std::map<std::string_view, std::vector<AILEnumeration>> applicationMapMTL;

static EnableAIL<IGFX_METEORLAKE> enableAILMTL;

constexpr std::array<std::string_view, 3> applicationsLegacyValidationPathMtl = {
    "blender", "bforartists", "cycles"};

template <>
void AILConfigurationHw<IGFX_METEORLAKE>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
    auto search = applicationMapMTL.find(processName);
    if (search != applicationMapMTL.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::disableDirectSubmission:
                runtimeCapabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported = false;
            default:
                break;
            }
        }
    }
}

template <>
bool AILConfigurationHw<IGFX_METEORLAKE>::isContextSyncFlagRequired() {
    auto iterator = applicationsContextSyncFlag.find(processName);
    return iterator != applicationsContextSyncFlag.end();
}

template <>
bool AILConfigurationHw<IGFX_METEORLAKE>::useLegacyValidationLogic() {
    auto it = std::find_if(applicationsLegacyValidationPathMtl.begin(), applicationsLegacyValidationPathMtl.end(), [this](const auto &appName) {
        return this->processName == appName;
    });
    return it != applicationsLegacyValidationPathMtl.end() ? true : false;
}

template <>
bool AILConfigurationHw<IGFX_METEORLAKE>::isBufferPoolEnabled() {
    auto iterator = applicationsBufferPoolDisabled.find(processName);
    return iterator == applicationsBufferPoolDisabled.end();
}

template class AILConfigurationHw<IGFX_METEORLAKE>;

} // namespace NEO
