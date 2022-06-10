/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"

#include <map>
namespace NEO {
static EnableAIL<IGFX_DG2> enableAILDG2;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapDG2 = {{"Wondershare Filmora 11", {AILEnumeration::DISABLE_BLITTER}}}; // Blitter is disabled as a temporary mitigation of high GPU utilization

template <>
inline void AILConfigurationHw<IGFX_DG2>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
    auto search = applicationMapDG2.find(processName);

    if (search != applicationMapDG2.end()) {
        for (size_t i = 0; i < search->second.size(); ++i) {
            switch (search->second[i]) {
            case AILEnumeration::DISABLE_BLITTER:
                runtimeCapabilityTable.blitterOperationsSupported = false;
                break;
            default:
                break;
            }
        }
    }
}
} // namespace NEO
