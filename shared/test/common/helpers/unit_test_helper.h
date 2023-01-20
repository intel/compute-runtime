/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aux_translation.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

#include <cstddef>
#include <vector>

namespace NEO {

class CommandStreamReceiver;
class LinearStream;
struct KernelDescriptor;
struct HardwareInfo;

template <typename GfxFamily>
struct UnitTestHelper {
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

    static bool isAdditionalMiSemaphoreWaitRequired(const HardwareInfo &hwInfo);

    static bool isAdditionalMiSemaphoreWait(const typename GfxFamily::MI_SEMAPHORE_WAIT &semaphoreWait);

    static uint64_t getAtomicMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic);

    static bool requiresTimestampPacketsInSystemMemory(HardwareInfo &hwInfo);

    static void setExtraMidThreadPreemptionFlag(HardwareInfo &hwInfo, bool value);

    static uint32_t getDebugModeRegisterOffset();
    static uint32_t getDebugModeRegisterValue();
    static uint32_t getTdCtlRegisterOffset();
    static uint32_t getTdCtlRegisterValue();

    static const bool tiledImagesSupported;

    static const uint32_t smallestTestableSimdSize;

    static const AuxTranslationMode requiredAuxTranslationMode;

    static const bool useFullRowForLocalIdsGeneration;

    static const bool additionalMiFlushDwRequired;

    static uint64_t getPipeControlPostSyncAddress(const typename GfxFamily::PIPE_CONTROL &pipeControl);
    static bool getPipeControlHdcPipelineFlush(const typename GfxFamily::PIPE_CONTROL &pipeControl);
    static void setPipeControlHdcPipelineFlush(typename GfxFamily::PIPE_CONTROL &pipeControl, bool hdcPipelineFlush);

    static void adjustKernelDescriptorForImplicitArgs(KernelDescriptor &kernelDescriptor);

    static std::vector<bool> getProgrammedLargeGrfValues(CommandStreamReceiver &csr, LinearStream &linearStream);

    static bool getWorkloadPartitionForStoreRegisterMemCmd(typename GfxFamily::MI_STORE_REGISTER_MEM &storeRegisterMem);

    static bool timestampRegisterHighAddress();

    static void validateSbaMocs(uint32_t expectedMocs, CommandStreamReceiver &csr);

    static GenCmdList::iterator findMidThreadPreemptionAllocationCommand(GenCmdList::iterator begin, GenCmdList::iterator end);

    static std::vector<GenCmdList::iterator> findAllMidThreadPreemptionAllocationCommand(GenCmdList::iterator begin, GenCmdList::iterator end);

    static bool getDisableFusionStateFromFrontEndCommand(const typename GfxFamily::VFE_STATE_TYPE &feCmd);
    static bool getComputeDispatchAllWalkerFromFrontEndCommand(const typename GfxFamily::VFE_STATE_TYPE &feCmd);
    static bool getSystolicFlagValueFromPipelineSelectCommand(const typename GfxFamily::PIPELINE_SELECT &pipelineSelectCmd);
    static size_t getAdditionalDshSize();
};

} // namespace NEO
