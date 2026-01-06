/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aux_translation.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

#include <cstddef>
#include <vector>

class DebugManagerStateRestore;

namespace WalkerPartition {

struct WalkerPartitionArgs;

} // namespace WalkerPartition

namespace NEO {

class CommandStreamReceiver;
class LinearStream;
struct DeviceInfo;
struct KernelDescriptor;
struct HardwareInfo;
struct RootDeviceEnvironment;

struct UnitTestSetter {
    static void disableHeapless(const DebugManagerStateRestore &restorer);
    static void disableHeaplessStateInit(const DebugManagerStateRestore &restorer);
    static void setCcsExposure(RootDeviceEnvironment &rootDeviceEnvironment);
    static void setRcsExposure(RootDeviceEnvironment &rootDeviceEnvironment);
};

template <typename GfxFamily>
struct UnitTestHelperWithHeap {
    static bool getDisableFusionStateFromFrontEndCommand(const typename GfxFamily::FrontEndStateCommand &feCmd);
    static bool getComputeDispatchAllWalkerFromFrontEndCommand(const typename GfxFamily::FrontEndStateCommand &feCmd);
};

template <typename GfxFamily>
struct UnitTestHelperNoHeap {
};

template <typename GfxFamily>
using UnitTestHelperBase = std::conditional_t<GfxFamily::isHeaplessRequired(), UnitTestHelperNoHeap<GfxFamily>, UnitTestHelperWithHeap<GfxFamily>>;

template <typename GfxFamily>
struct UnitTestHelper : public UnitTestHelperBase<GfxFamily> {
    using COHERENCY_TYPE = typename GfxFamily::RENDER_SURFACE_STATE::COHERENCY_TYPE;

    static bool isL3ConfigProgrammable();

    static bool evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, const KernelDescriptor *kernelDescriptor, uint32_t rootDeviceIndex);

    static bool isPageTableManagerSupported(const HardwareInfo &hwInfo);

    static bool isTimestampPacketWriteSupported();

    static bool isExpectMemoryNotEqualSupported();

    static uint32_t getDefaultSshUsage();

    static uint32_t getAppropriateThreadArbitrationPolicy(int32_t policy);

    static auto getCoherencyTypeSupported(COHERENCY_TYPE coherencyType) -> decltype(coherencyType);

    static bool evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress);

    static bool isPipeControlWArequired(const HardwareInfo &hwInfo);

    static bool isAdditionalSynchronizationRequired();

    static bool isAdditionalMiSemaphoreWaitRequired(const RootDeviceEnvironment &rootDeviceEnvironment);

    static bool isAdditionalMiSemaphoreWait(const typename GfxFamily::MI_SEMAPHORE_WAIT &semaphoreWait);

    static uint64_t getAtomicMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic);

    static bool requiresTimestampPacketsInSystemMemory(HardwareInfo &hwInfo);

    static uint32_t getDebugModeRegisterOffset();
    static uint32_t getDebugModeRegisterValue();
    static uint32_t getTdCtlRegisterOffset();
    static uint32_t getTdCtlRegisterValue();

    static uint32_t getMiLoadRegisterImmProgrammedCmdsCount(bool debuggingEnabled);

    static const uint32_t smallestTestableSimdSize;

    static const AuxTranslationMode requiredAuxTranslationMode;

    static const bool useFullRowForLocalIdsGeneration;

    static const bool additionalMiFlushDwRequired;

    static uint64_t getPipeControlPostSyncAddress(const typename GfxFamily::PIPE_CONTROL &pipeControl);
    static bool getPipeControlHdcPipelineFlush(const typename GfxFamily::PIPE_CONTROL &pipeControl);
    static void setPipeControlHdcPipelineFlush(typename GfxFamily::PIPE_CONTROL &pipeControl, bool hdcPipelineFlush);

    static void adjustKernelDescriptorForImplicitArgs(KernelDescriptor &kernelDescriptor);

    static std::vector<bool> getProgrammedLargeGrfValues(CommandStreamReceiver &csr, LinearStream &linearStream);

    static std::vector<bool> getProgrammedLargeGrfValues(LinearStream &linearStream);

    static uint32_t getProgrammedGrfValue(CommandStreamReceiver &csr, LinearStream &linearStream);

    static bool getWorkloadPartitionForStoreRegisterMemCmd(typename GfxFamily::MI_STORE_REGISTER_MEM &storeRegisterMem);

    static bool timestampRegisterHighAddress();

    static void validateSbaMocs(uint32_t expectedMocs, CommandStreamReceiver &csr);

    static GenCmdList::iterator findCsrBaseAddressCommand(GenCmdList::iterator begin, GenCmdList::iterator end);

    static std::vector<GenCmdList::iterator> findAllMidThreadPreemptionAllocationCommand(GenCmdList::iterator begin, GenCmdList::iterator end);
    static uint32_t getInlineDataSize(bool isHeaplessEnabled);
    static bool getSystolicFlagValueFromPipelineSelectCommand(const typename GfxFamily::PIPELINE_SELECT &pipelineSelectCmd);
    static size_t getAdditionalDshSize(uint32_t iddCount);
    static bool expectNullDsh(const DeviceInfo &deviceInfo);

    static bool findStateCacheFlushPipeControl(CommandStreamReceiver &csr, LinearStream &csrStream);
    static void verifyDummyBlitWa(const RootDeviceEnvironment *rootDeviceEnvironment, GenCmdList::iterator &cmdIterator);
    static uint64_t getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(bool isHeaplessEnabled, WalkerPartition::WalkerPartitionArgs &testArgs);
    static GenCmdList::iterator findWalkerTypeCmd(GenCmdList::iterator begin, GenCmdList::iterator end);
    static std::vector<GenCmdList::iterator> findAllWalkerTypeCmds(GenCmdList::iterator begin, GenCmdList::iterator end);
    static void getSpaceAndInitWalkerCmd(LinearStream &stream, bool heapless);
    static void *getInitWalkerCmd(bool heapless);
    static size_t getWalkerSize(bool isHeaplessEnabled);
    template <typename WalkerType>
    static uint64_t getWalkerActivePostSyncAddress(WalkerType *walkerCmd);
    static void skipStatePrefetch(GenCmdList::iterator &iter);

    static bool isHeaplessAllowed();
};

} // namespace NEO
