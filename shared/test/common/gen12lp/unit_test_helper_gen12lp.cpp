/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/gen12lp/hw_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"

namespace NEO {

using Family = Gen12LpFamily;

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getAtomicMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic) {
    return atomic.getMemoryAddress() | ((static_cast<uint64_t>(atomic.getMemoryAddressHigh())) << 32);
}

template <typename GfxFamily>
const uint32_t UnitTestHelper<GfxFamily>::smallestTestableSimdSize = 8;

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getAppropriateThreadArbitrationPolicy(int32_t policy) {
    return static_cast<uint32_t>(policy);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalMiSemaphoreWaitRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return false;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getDebugModeRegisterValue() {
    return (1u << 5) | (1u << 21);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalSynchronizationRequired() {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::requiresTimestampPacketsInSystemMemory(HardwareInfo &hwInfo) {
    return !hwInfo.featureTable.flags.ftrLocalMemory;
}

template <typename GfxFamily>
const AuxTranslationMode UnitTestHelper<GfxFamily>::requiredAuxTranslationMode = AuxTranslationMode::builtin;

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getTdCtlRegisterValue() {
    return (1u << 7) | (1u << 4);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isL3ConfigProgrammable() {
    return true;
};

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::timestampRegisterHighAddress() {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, const KernelDescriptor *kernelDescriptor, uint32_t rootDeviceIndex) {
    if (sizeBeforeEnqueue != sizeAfterEnqueue) {
        return true;
    }
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isTimestampPacketWriteSupported() {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isExpectMemoryNotEqualSupported() {
    return false;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getDefaultSshUsage() {
    return sizeof(typename GfxFamily::RENDER_SURFACE_STATE);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalMiSemaphoreWait(const typename GfxFamily::MI_SEMAPHORE_WAIT &semaphoreWait) {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress) {
    return usedScratchGpuAddress == retrievedGshAddress;
}

template <typename GfxFamily>
auto UnitTestHelper<GfxFamily>::getCoherencyTypeSupported(COHERENCY_TYPE coherencyType) -> decltype(coherencyType) {
    return coherencyType;
}

template <typename GfxFamily>
inline bool UnitTestHelper<GfxFamily>::getPipeControlHdcPipelineFlush(const typename GfxFamily::PIPE_CONTROL &pipeControl) {
    return false;
}

template <typename GfxFamily>
inline void UnitTestHelper<GfxFamily>::setPipeControlHdcPipelineFlush(typename GfxFamily::PIPE_CONTROL &pipeControl, bool hdcPipelineFlush) {}

template <typename GfxFamily>
inline void UnitTestHelper<GfxFamily>::adjustKernelDescriptorForImplicitArgs(KernelDescriptor &kernelDescriptor) {
    kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer = 0u;
}

template <typename GfxFamily>
std::vector<bool> UnitTestHelper<GfxFamily>::getProgrammedLargeGrfValues(CommandStreamReceiver &csr, LinearStream &linearStream) {
    return {};
}

template <typename GfxFamily>
std::vector<bool> UnitTestHelper<GfxFamily>::getProgrammedLargeGrfValues(LinearStream &linearStream) {
    return {};
}

template <typename GfxFamily>
inline bool UnitTestHelper<GfxFamily>::getWorkloadPartitionForStoreRegisterMemCmd(typename GfxFamily::MI_STORE_REGISTER_MEM &storeRegisterMem) {
    return false;
}

template <typename GfxFamily>
GenCmdList::iterator UnitTestHelper<GfxFamily>::findCsrBaseAddressCommand(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return find<typename GfxFamily::GPGPU_CSR_BASE_ADDRESS *>(begin, end);
}

template <typename GfxFamily>
std::vector<GenCmdList::iterator> UnitTestHelper<GfxFamily>::findAllMidThreadPreemptionAllocationCommand(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return findAll<typename GfxFamily::GPGPU_CSR_BASE_ADDRESS *>(begin, end);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::getSystolicFlagValueFromPipelineSelectCommand(const typename GfxFamily::PIPELINE_SELECT &pipelineSelectCmd) {
    return false;
}

template <typename GfxFamily>
size_t UnitTestHelper<GfxFamily>::getAdditionalDshSize(uint32_t iddCount) {
    return iddCount * sizeof(typename GfxFamily::INTERFACE_DESCRIPTOR_DATA);
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::verifyDummyBlitWa(const RootDeviceEnvironment *rootDeviceEnvironment, GenCmdList::iterator &cmdIterator) {
}

template <typename GfxFamily>
GenCmdList::iterator UnitTestHelper<GfxFamily>::findWalkerTypeCmd(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return find<typename GfxFamily::GPGPU_WALKER *>(begin, end);
}
template <typename GfxFamily>
std::vector<GenCmdList::iterator> UnitTestHelper<GfxFamily>::findAllWalkerTypeCmds(GenCmdList::iterator begin, GenCmdList::iterator end) {
    return findAll<typename GfxFamily::GPGPU_WALKER *>(begin, end);
}

template <typename GfxFamily>
uint64_t UnitTestHelper<GfxFamily>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(bool isHeaplessEnabled, WalkerPartition::WalkerPartitionArgs &testArgs) {
    UNRECOVERABLE_IF(true);
    return 0u;
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::getSpaceAndInitWalkerCmd(LinearStream &stream, bool heapless) {
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    *stream.getSpaceForCmd<GPGPU_WALKER>() = GfxFamily::template getInitGpuWalker<GPGPU_WALKER>();
}

template <typename GfxFamily>
void *UnitTestHelper<GfxFamily>::getInitWalkerCmd(bool heapless) {
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    return new GPGPU_WALKER;
}

template <>
bool UnitTestHelper<Family>::isL3ConfigProgrammable() {
    return false;
};

template <>
bool UnitTestHelper<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
bool UnitTestHelper<Family>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return SpecialUltHelperGen12lp::isPipeControlWArequired(hwInfo.platform.eProductFamily);
}

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterOffset() {
    return 0x20d8;
}

template <>
bool UnitTestHelperWithHeap<Family>::getDisableFusionStateFromFrontEndCommand(const typename Family::FrontEndStateCommand &feCmd) {
    return feCmd.getDisableSlice0Subslice2();
}

template <>
bool UnitTestHelper<Family>::getSystolicFlagValueFromPipelineSelectCommand(const typename Family::PIPELINE_SELECT &pipelineSelectCmd) {
    return pipelineSelectCmd.getSpecialModeEnable();
}

template <>
template <typename WalkerType>
uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress(WalkerType *walkerCmd) {
    return 0;
}

template <>
void UnitTestHelper<Family>::skipStatePrefetch(GenCmdList::iterator &iter) {}

template struct UnitTestHelper<Family>;
template struct UnitTestHelperWithHeap<Family>;

template uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress<Family::GPGPU_WALKER>(Family::GPGPU_WALKER *walkerCmd);
} // namespace NEO
