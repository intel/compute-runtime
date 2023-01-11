/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <>
std::string ProductHelperHw<gfxProduct>::getDeviceMemoryName() const {
    return "HBM";
}

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isDcFlushAllowed() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isFlushTaskAllowed() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return true;
}

template <>
std::pair<bool, bool> ProductHelperHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    auto isBasicWARequired = false;
    auto isExtendedWARequired = false;

    if (DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        isExtendedWARequired = DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get();
    }

    return {isBasicWARequired, isExtendedWARequired};
}

template <>
bool ProductHelperHw<gfxProduct>::isTlbFlushRequired() const {
    return false;
}

template <>
void ProductHelperHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) const {
    using SAMPLER_STATE = typename XeHpcCoreFamily::SAMPLER_STATE;

    auto samplerState = reinterpret_cast<SAMPLER_STATE *>(sampler);
    if (DebugManager.flags.ForceSamplerLowFilteringPrecision.get()) {
        samplerState->setLowQualityFilter(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE);
    }
}

template <>
void ProductHelperHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) const {
    *fp16 = (FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE | FP_ATOMIC_EXT_FLAG_GLOBAL_ADD | FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    *fp32 = (FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE | FP_ATOMIC_EXT_FLAG_GLOBAL_ADD | FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
    *fp64 = (FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE | FP_ATOMIC_EXT_FLAG_GLOBAL_ADD | FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX);
}

template <>
bool ProductHelperHw<gfxProduct>::isPrefetcherDisablingInDirectSubmissionRequired() const {
    return false;
}
} // namespace NEO
