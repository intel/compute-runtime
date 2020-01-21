/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_helper_bdw_plus.inl"
#include "runtime/aub/aub_helper_bdw_plus.inl"
#include "runtime/gen12lp/helpers_gen12lp.h"
#include "runtime/helpers/flat_batch_buffer_helper_hw.inl"

#include "engine_node.h"

namespace NEO {
typedef TGLLPFamily Family;

template <>
bool HwHelperHw<Family>::isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const {
    return Gen12LPHelpers::isOffsetToSkipSetFFIDGPWARequired(hwInfo);
}

template <>
bool HwHelperHw<Family>::isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo) {
    return Gen12LPHelpers::isForceDefaultRCSEngineWARequired(hwInfo);
}

template <>
bool HwHelperHw<Family>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) {
    return Gen12LPHelpers::isForceEmuInt32DivRemSPWARequired(hwInfo);
}

template <>
void HwHelperHw<Family>::adjustDefaultEngineType(HardwareInfo *pHwInfo) {
    if (!pHwInfo->featureTable.ftrCCSNode || isForceDefaultRCSEngineWARequired(*pHwInfo)) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    }
}

template <>
uint32_t HwHelperHw<Family>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    /* For ICL+ maxThreadCount equals (EUCount * 8).
     ThreadCount/EUCount=7 is no longer valid, so we have to force 8 in below formula.
     This is required to allocate enough scratch space. */
    return pHwInfo->gtSystemInfo.MaxSubSlicesSupported * pHwInfo->gtSystemInfo.MaxEuPerSubSlice * 8;
}

template <>
bool HwHelperHw<Family>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) const {
    return Gen12LPHelpers::isLocalMemoryEnabled(hwInfo);
}

template <>
bool HwHelperHw<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
bool HwHelperHw<Family>::obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size) const {
    return false;
}

template <>
bool HwHelperHw<Family>::checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) {
    if (graphicsAllocation.getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED) {
        return false;
    }
    return true;
}

template <>
void HwHelperHw<Family>::setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) {
    coherencyFlag = true;
    if (pHwInfo->platform.eProductFamily == IGFX_TIGERLAKE_LP && pHwInfo->platform.usRevId == 0x0) {
        //stepping A0 devices - turn off coherency
        coherencyFlag = false;
    }

    Gen12LPHelpers::adjustCoherencyFlag(pHwInfo->platform.eProductFamily, coherencyFlag);
}

template <>
uint32_t HwHelperHw<Family>::getPitchAlignmentForImage(const HardwareInfo *hwInfo) {
    if (Gen12LPHelpers::imagePitchAlignmentWaRequired(hwInfo->platform.eProductFamily)) {
        auto stepping = hwInfo->platform.usRevId;
        if (stepping == 0) {
            return 64u;
        }
        return 4u;
    }
    return 4u;
}

template <>
uint32_t HwHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen12);
}

template <>
const std::vector<aub_stream::EngineType> HwHelperHw<Family>::getGpgpuEngineInstances() const {
    constexpr std::array<aub_stream::EngineType, 4> gpgpuEngineInstances = {{aub_stream::ENGINE_RCS,
                                                                             aub_stream::ENGINE_RCS, // low priority
                                                                             aub_stream::ENGINE_RCS, // internal usage
                                                                             aub_stream::ENGINE_CCS}};
    return std::vector<aub_stream::EngineType>(gpgpuEngineInstances.begin(), gpgpuEngineInstances.end());
};

template <>
void PipeControlHelper<Family>::addPipeControlWA(LinearStream &commandStream, const HardwareInfo &hwInfo) {
    if (Gen12LPHelpers::pipeControlWaRequired(hwInfo.platform.eProductFamily)) {
        auto stepping = hwInfo.platform.usRevId;
        if (stepping == 0) {
            auto pCmd = static_cast<Family::PIPE_CONTROL *>(commandStream.getSpace(sizeof(Family::PIPE_CONTROL)));
            *pCmd = Family::cmdInitPipeControl;
            pCmd->setCommandStreamerStallEnable(true);
        }
    }
}

template class AubHelperHw<Family>;
template class HwHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct PipeControlHelper<Family>;
} // namespace NEO
