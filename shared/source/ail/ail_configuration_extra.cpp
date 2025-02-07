/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"

#include <map>
#include <vector>

namespace NEO {
/*
 * fp64 support is unavailable on some Intel GPUs, and the SW emulation in IGC should not be enabled by default.
 * For Blender, fp64 is not performance-critical - SW emulation is good enough for the application to be usable
 * (some versions would not function correctly without it).
 *
 */

std::map<std::string_view, std::vector<AILEnumeration>> applicationMap = {{"blender", {AILEnumeration::enableFp64}},
                                                                          // Modify reported platform name to ensure older versions of Adobe Premiere Pro are able to recognize the GPU device
                                                                          {"Adobe Premiere Pro", {AILEnumeration::enableLegacyPlatformName}}};

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapMTL = {{"svchost", {AILEnumeration::disableDirectSubmission}},
                                                                             {"aomhost64", {AILEnumeration::disableDirectSubmission}},
                                                                             {"Zoom", {AILEnumeration::disableDirectSubmission}}};

const std::set<std::string_view> applicationsForceRcsDg2 = {};

const std::set<std::string_view> applicationsContextSyncFlag = {};

const std::set<std::string_view> applicationsBufferPoolDisabled = {};

const std::set<std::string_view> applicationsBufferPoolDisabledXe = {};

const std::set<std::string_view> applicationsOverfetchDisabled = {};

const std::set<std::string_view> applicationsDrainHostptrsDisabled = {};

const std::set<std::string_view> applicationsDeviceUSMRecyclingLimited = {};

const std::set<std::string_view> applicationsMicrosecontResolutionAdjustment = {};

const uint32_t microsecondAdjustment = 1000;

AILConfigurationCreateFunctionType ailConfigurationFactory[IGFX_MAX_PRODUCT];

void AILConfiguration::apply(HardwareInfo &hwInfo) {
    auto search = applicationMap.find(processName);

    if (search != applicationMap.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::enableFp64:
                hwInfo.capabilityTable.ftrSupportsFP64 = true;
                break;
            case AILEnumeration::enableLegacyPlatformName:
                hwInfo.capabilityTable.preferredPlatformName = legacyPlatformName;
                break;
            default:
                break;
            }
        }
    }

    applyExt(hwInfo);
}

} // namespace NEO
