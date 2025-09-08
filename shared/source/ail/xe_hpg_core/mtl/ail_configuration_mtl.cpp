/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"

#include "aubstream/engine_node.h"

#include <algorithm>
#include <map>
#include <vector>

constexpr static auto gfxProduct = IGFX_METEORLAKE;

#include "shared/source/ail/ail_configuration_tgllp_and_later.inl"

namespace NEO {

extern std::map<std::string_view, std::vector<AILEnumeration>> applicationMapMTL;

static EnableAIL<gfxProduct> enableAILMTL;

constexpr std::array<std::string_view, 3> applicationsLegacyValidationPathMtl = {
    "blender", "bforartists", "cycles"};

template <>
void AILConfigurationHw<gfxProduct>::applyExt(HardwareInfo &hwInfo) {
    auto search = applicationMapMTL.find(processName);
    if (search != applicationMapMTL.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::disableDirectSubmission:
                hwInfo.capabilityTable.directSubmissionEngines.data[aub_stream::ENGINE_CCS].engineSupported = false;
                break;
            case AILEnumeration::directSubmissionControllerConfig:
                debugManager.flags.DirectSubmissionControllerTimeout.set(1000);
                debugManager.flags.DirectSubmissionControllerMaxTimeout.set(1000);
            default:
                break;
            }
        }
    }
}

template <>
bool AILConfigurationHw<gfxProduct>::isContextSyncFlagRequired() {
    auto iterator = applicationsContextSyncFlag.find(processName);
    return iterator != applicationsContextSyncFlag.end();
}

template <>
bool AILConfigurationHw<gfxProduct>::useLegacyValidationLogic() {
    auto it = std::find_if(applicationsLegacyValidationPathMtl.begin(), applicationsLegacyValidationPathMtl.end(), [this](const auto &appName) {
        return this->processName == appName;
    });
    return it != applicationsLegacyValidationPathMtl.end() ? true : false;
}

template <>
bool AILConfigurationHw<gfxProduct>::isBufferPoolEnabled() {
    auto iterator = applicationsBufferPoolDisabled.find(processName);
    return iterator == applicationsBufferPoolDisabled.end();
}

template <>
bool AILConfigurationHw<gfxProduct>::limitAmountOfDeviceMemoryForRecycling() {
    auto iterator = applicationsDeviceUSMRecyclingLimited.find(processName);
    return iterator != applicationsDeviceUSMRecyclingLimited.end();
}

template class AILConfigurationHw<gfxProduct>;

} // namespace NEO
