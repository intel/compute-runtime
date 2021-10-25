/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_mem_dump.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::adjustSamplerState(void *sampler, const HardwareInfo &hwInfo) {
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::convertTimestampsFromOaToCsDomain(uint64_t &timestampData) {
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getMaxThreadsForWorkgroupInDSSOrSS(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice, uint32_t maxNumEUsPerDualSubSlice) const {
    return 0;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const {
    return 0;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::setForceNonCoherent(void *const commandPtr, const StateComputeModeProperties &properties) {
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getSharedSystemMemCapabilities() {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getHostMemCapabilities(const HardwareInfo *hwInfo) {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemCapabilities() {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getSingleDeviceSharedMemCapabilities() {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getCrossDeviceSharedMemCapabilities() {
    return 0;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemoryMaxClkRate(const HardwareInfo *hwInfo) {
    return 0;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isAdditionalStateBaseAddressWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isMaxThreadsForWorkgroupWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getHwRevIdFromStepping(uint32_t stepping, const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
    return CommonConstants::invalidStepping;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getAubStreamSteppingFromHwRevId(const HardwareInfo &hwInfo) const {
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
template <>
std::string HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemoryName() const {
    return "";
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::setAdditionalPipelineSelectFields(void *pipelineSelectCmd, const PipelineSelectArgs &pipelineSelectArgs, const HardwareInfo &hwInfo) {
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isDefaultEngineTypeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isDisableOverdispatchAvailable(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::allowRenderCompression(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::allowStatelessCompression(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
LocalMemoryAccessMode HwInfoConfigHw<IGFX_UNKNOWN>::getDefaultLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::Default;
}

template <>
LocalMemoryAccessMode HwInfoConfigHw<IGFX_UNKNOWN>::getLocalMemoryAccessMode(const HardwareInfo &hwInfo) const {
    return LocalMemoryAccessMode::Default;
}

template <>
std::vector<uint32_t> HwInfoConfigHw<IGFX_UNKNOWN>::getKernelSupportedThreadArbitrationPolicies() {
    return std::vector<uint32_t>();
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isAllocationSizeAdjustmentRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isPrefetchDisablingRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isNewResidencyModelSupported() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::heapInLocalMem(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) {
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isAdditionalMediaSamplerProgrammingRequired() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isInitialFlagsProgrammingRequired() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isReturnedCmdSizeForMediaSamplerAdjustmentRequired() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::extraParametersInvalid(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::pipeControlWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::imagePitchAlignmentWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::is3DPipelineSelectWARequired() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isStorageInfoAdjustmentRequired() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isBlitterForImagesSupported() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isTile64With3DSurfaceOnBCSSupported(const HardwareInfo &hwInfo) const {
    return false;
}

} //namespace NEO
