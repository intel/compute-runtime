/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration_base.inl"
#include "shared/source/helpers/hw_info.h"

#include <algorithm>
#include <map>

namespace NEO {

static EnableAIL<IGFX_DG2> enableAILDG2;

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

constexpr std::array<std::string_view, 3> applicationsLegacyValidationPathDg2 = {
    "blender", "bforartists", "cycles"};

template <>
void AILConfigurationHw<IGFX_DG2>::modifyKernelIfRequired(std::string &kernelsSources) {

    auto it = std::find_if(applicationsKernelFixesDG2.begin(), applicationsKernelFixesDG2.end(), [this](const auto &param) {
        return this->processName == param.applicationName;
    });

    if (it != applicationsKernelFixesDG2.end()) {

        if (sourcesContain(kernelsSources, it->kernelName) && isKernelHashCorrect(kernelsSources, it->kernelHash)) {
            kernelsSources.insert(it->fixStartPosition, it->fixCode);
        }
    }
}

template <>
bool AILConfigurationHw<IGFX_DG2>::useLegacyValidationLogic() {
    auto it = std::find_if(applicationsLegacyValidationPathDg2.begin(), applicationsLegacyValidationPathDg2.end(), [this](const auto &appName) {
        return this->processName == appName;
    });
    return it != applicationsLegacyValidationPathDg2.end() ? true : false;
}

template <>
inline void AILConfigurationHw<IGFX_DG2>::applyExt(HardwareInfo &hwInfo) {
    auto search = applicationsForceRcsDg2.find(processName);
    if (search != applicationsForceRcsDg2.end()) {
        shouldForceRcs = true;
    }
}

template <>
bool AILConfigurationHw<IGFX_DG2>::isBufferPoolEnabled() {
    auto iterator = applicationsBufferPoolDisabledXe.find(processName);
    return iterator == applicationsBufferPoolDisabledXe.end();
}

template class AILConfigurationHw<IGFX_DG2>;

} // namespace NEO
