/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
int HwInfoConfigHw<gfxProduct>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    enableRenderCompression(hwInfo);
    enableBlitterOperationsSupport(hwInfo);

    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
    *fp16 = 0u;
    *fp32 = 0u;
    *fp64 = 0u;
}

template <PRODUCT_FAMILY gfxProduct>
std::vector<uint32_t> HwInfoConfigHw<gfxProduct>::getKernelSupportedThreadArbitrationPolicies() {
    using GfxFamily = typename HwMapper<gfxProduct>::GfxFamily;
    return PreambleHelper<GfxFamily>::getSupportedThreadArbitrationPolicies();
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getSharedSystemMemCapabilities() {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::convertTimestampsFromOaToCsDomain(uint64_t &timestampData){};

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::enableBlitterOperationsSupport(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.blitterOperationsSupported = obtainBlitterPreference(*hwInfo);

    if (DebugManager.flags.EnableBlitterOperationsSupport.get() != -1) {
        hwInfo->capabilityTable.blitterOperationsSupported = !!DebugManager.flags.EnableBlitterOperationsSupport.get();
    }
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getDeviceMemCapabilities() {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getSingleDeviceSharedMemCapabilities() {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::getHostMemCapabilitiesSupported(const HardwareInfo *hwInfo) {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getHostMemCapabilities(const HardwareInfo *hwInfo) {
    bool supported = getHostMemCapabilitiesSupported(hwInfo);

    if (DebugManager.flags.EnableHostUsmSupport.get() != -1) {
        supported = !!DebugManager.flags.EnableHostUsmSupport.get();
    }

    return (supported ? getHostMemCapabilitiesValue() : 0);
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) {
    return 0u;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const {
    uint32_t numThreadsPerEU = hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;
    return maxNumEUsPerSubSlice * numThreadsPerEU;
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) {}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t HwInfoConfigHw<gfxProduct>::getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    switch (getSteppingFromHwRevId(hwInfo)) {
    default:
    case REVISION_A0:
    case REVISION_A1:
    case REVISION_A3:
        return AubMemDump::SteppingValues::A;
    case REVISION_B:
        return AubMemDump::SteppingValues::B;
    case REVISION_C:
        return AubMemDump::SteppingValues::C;
    case REVISION_D:
        return AubMemDump::SteppingValues::D;
    case REVISION_K:
        return AubMemDump::SteppingValues::K;
    }
}

template <PRODUCT_FAMILY gfxProduct>
void HwInfoConfigHw<gfxProduct>::setAdditionalPipelineSelectFields(void *pipelineSelectCmd, const PipelineSelectArgs &pipelineSelectArgs, const HardwareInfo &hwInfo) {}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
std::string HwInfoConfigHw<gfxProduct>::getDeviceMemoryName() const {
    return "DDR";
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::allowRenderCompression(const HardwareInfo &hwInfo) const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::allowStatelessCompression(const HardwareInfo &hwInfo) const {
    if (!NEO::ApiSpecificConfig::isStatelessCompressionSupported()) {
        return false;
    }
    if (DebugManager.flags.EnableStatelessCompression.get() != -1) {
        return static_cast<bool>(DebugManager.flags.EnableStatelessCompression.get());
    }
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
LocalMemoryAccessMode HwInfoConfigHw<gfxProduct>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::Default;
}

template <PRODUCT_FAMILY gfxProduct>
LocalMemoryAccessMode HwInfoConfigHw<gfxProduct>::getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    switch (static_cast<LocalMemoryAccessMode>(DebugManager.flags.ForceLocalMemoryAccessMode.get())) {
    case LocalMemoryAccessMode::Default:
    case LocalMemoryAccessMode::CpuAccessAllowed:
    case LocalMemoryAccessMode::CpuAccessDisallowed:
        return static_cast<LocalMemoryAccessMode>(DebugManager.flags.ForceLocalMemoryAccessMode.get());
    }
    return getDefaultLocalMemoryAccessMode(hwInfo);
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAllocationSizeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isPrefetchDisablingRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isAdditionalMediaSamplerProgrammingRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isInitialFlagsProgrammingRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::extraParametersInvalid(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::is3DPipelineSelectWARequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isStorageInfoAdjustmentRequired() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool HwInfoConfigHw<gfxProduct>::isBlitterForImagesSupported() const {
    return false;
}
} // namespace NEO
