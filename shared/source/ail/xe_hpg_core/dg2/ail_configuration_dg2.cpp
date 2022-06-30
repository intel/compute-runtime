/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/ail/ail_configuration_base.inl"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace NEO {
static EnableAIL<IGFX_DG2> enableAILDG2;

std::map<std::string_view, std::vector<AILEnumeration>> applicationMapDG2 = {
    {"Wondershare Filmora 11", {AILEnumeration::DISABLE_BLITTER}}, // Blitter is disabled as a temporary mitigation of high GPU utilization
    {"perf_check", {AILEnumeration::DISABLE_BLITTER}},             // perf_check
    {"tlb_player_gui", {AILEnumeration::DISABLE_BLITTER}}          // and tlb_player_gui are part of Wondershare Filmora 11
};

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

struct ApplicationKernelFixDg2 {
    std::string_view applicationName;
    std::string_view kernelName;
    uint64_t kernelHash;
    size_t fixStartPosition;
    std::string_view fixCode;
};

// There is a known functional bug in OpenMM that was recently fixed (https://github.com/openmm/openmm/commit/7af08783e08d3219c1a5f5aa3eff18f8421a9d83)
// FAHbench is known to use older version of OpenMM (containing a bug) - we patch this kernel by injecting the missing barrier to ensure it's functionally correct on DG2.

const std::vector<ApplicationKernelFixDg2> applicationsKernelFixesDG2 =
    {{"FAHBench-gui", "findBlocksWithInteractions", 0xa39732fc26656899, 12651u, "else { SYNC_WARPS; }"},
     {"FAHBench-cmd", "findBlocksWithInteractions", 0xa39732fc26656899, 12651u, "else { SYNC_WARPS; }"}};

template <>
void AILConfigurationHw<IGFX_DG2>::modifyKernelIfRequired(std::string &kernelsSources) {

    auto it = std::find_if(applicationsKernelFixesDG2.begin(), applicationsKernelFixesDG2.end(), [this](const auto &param) {
        return this->processName == param.applicationName;
    });

    if (it != applicationsKernelFixesDG2.end()) {

        if (sourcesContainKernel(kernelsSources, it->kernelName) && isKernelHashCorrect(kernelsSources, it->kernelHash)) {
            kernelsSources.insert(it->fixStartPosition, it->fixCode);
        }
    }
}

template class AILConfigurationHw<IGFX_DG2>;

} // namespace NEO
