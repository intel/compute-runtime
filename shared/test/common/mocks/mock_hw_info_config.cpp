/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "platforms.h"

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
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getSharedSystemMemCapabilities(const HardwareInfo *hwInfo) {
    return 0;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::getKernelExtendedProperties(uint32_t *fp16, uint32_t *fp32, uint32_t *fp64) {
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::overrideGfxPartitionLayoutForWsl() const {
    return false;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemoryMaxClkRate(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemoryPhysicalSizeInBytes(const OSInterface *osIface, uint32_t subDeviceIndex) {
    return 0;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemoryMaxBandWidthInBytesPerSecond(const HardwareInfo &hwInfo, const OSInterface *osIface, uint32_t subDeviceIndex) {
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
bool HwInfoConfigHw<IGFX_UNKNOWN>::isBlitterFullySupported(const HardwareInfo &hwInfo) const {
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
AOT::PRODUCT_CONFIG HwInfoConfigHw<IGFX_UNKNOWN>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return AOT::UNKNOWN_ISA;
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
bool HwInfoConfigHw<IGFX_UNKNOWN>::allowCompression(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::allowStatelessCompression(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isDirectSubmissionSupported(const HardwareInfo &hwInfo) const {
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
std::vector<int32_t> HwInfoConfigHw<IGFX_UNKNOWN>::getKernelSupportedThreadArbitrationPolicies() {
    return {};
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
std::pair<bool, bool> HwInfoConfigHw<IGFX_UNKNOWN>::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    return {false, false};
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

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isDcFlushAllowed() const {
    return true;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::computeMaxNeededSubSliceSpace(const HardwareInfo &hwInfo) const {
    return hwInfo.gtSystemInfo.MaxSubSlicesSupported;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::getUuid(Device *device, std::array<uint8_t, HwInfoConfig::uuidSize> &uuid) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isFlushTaskAllowed() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::programAllStateComputeCommandFields() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isSpecialPipelineSelectModeChanged(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isSystolicModeConfigurable(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isComputeDispatchAllWalkerEnableInComputeWalkerRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isGlobalFenceInCommandStreamRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isAdjustProgrammableIdPreferredSlmSizeRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
uint32_t HwInfoConfigHw<IGFX_UNKNOWN>::getThreadEuRatioForScratch(const HardwareInfo &hwInfo) const {
    return 8u;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isComputeDispatchAllWalkerEnableInCfeStateRequired(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isIpSamplingSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isVmBindPatIndexProgrammingSupported() const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isBFloat16ConversionSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isMatrixMultiplyAccumulateSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::updateScmCommand(void *const commandPtr, const StateComputeModeProperties &properties) {
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::updateIddCommand(void *const commandPtr, uint32_t numGrf, int32_t threadArbitrationPolicy) {
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isGrfNumReportedWithScm() const {
    return false;
}
template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::enableCompression(HardwareInfo *hwInfo) {
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isCooperativeEngineSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool HwInfoConfigHw<IGFX_UNKNOWN>::isTimestampWaitSupportedForEvents() const {
    return false;
}

template <>
uint64_t HwInfoConfigHw<IGFX_UNKNOWN>::getHostMemCapabilitiesValue() {
    return 0;
}

template <>
const char *L1CachePolicyHelper<IGFX_UNKNOWN>::getCachingPolicyOptions() {
    return nullptr;
}

template <>
uint32_t L1CachePolicyHelper<IGFX_UNKNOWN>::getDefaultL1CachePolicy() {
    return 0u;
}

} // namespace NEO

#include "shared/source/os_interface/hw_info_config.inl"

template class NEO::HwInfoConfigHw<IGFX_UNKNOWN>;
template struct NEO::L1CachePolicyHelper<IGFX_UNKNOWN>;
