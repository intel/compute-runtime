/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/command_stream/csr_properties_flags.h"
#include "shared/source/command_stream/memory_compression_state.h"
#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/kernel/kernel_execution_type.h"

#include <limits>

namespace NEO {
struct FlushStampTrackingObj;

namespace CSRequirements {
//cleanup section usually contains 1-2 pipeControls BB end and place for BB start
//that makes 16 * 2 + 4 + 8 = 40 bytes
//then command buffer is aligned to cacheline that can take up to 63 bytes
//to be sure everything fits minimal size is at 2 x cacheline.

constexpr auto minCommandQueueCommandStreamSize = 2 * MemoryConstants::cacheLineSize;
constexpr auto csOverfetchSize = MemoryConstants::pageSize;
} // namespace CSRequirements

namespace TimeoutControls {
constexpr int64_t maxTimeout = std::numeric_limits<int64_t>::max();
}

namespace QueueSliceCount {
constexpr uint64_t defaultSliceCount = 0;
}

namespace L3CachingSettings {
constexpr uint32_t l3CacheOn = 0u;
constexpr uint32_t l3CacheOff = 1u;
constexpr uint32_t l3AndL1On = 2u;
constexpr uint32_t NotApplicable = 3u;
} // namespace L3CachingSettings

struct DispatchFlags {
    DispatchFlags() = delete;
    DispatchFlags(CsrDependencies csrDependenciesP, TimestampPacketContainer *barrierTimestampPacketNodesP, PipelineSelectArgs pipelineSelectArgsP,
                  FlushStampTrackingObj *flushStampReferenceP, QueueThrottle throttleP, PreemptionMode preemptionModeP, uint32_t numGrfRequiredP,
                  uint32_t l3CacheSettingsP, int32_t threadArbitrationPolicyP, uint32_t additionalKernelExecInfoP,
                  KernelExecutionType kernelExecutionTypeP, MemoryCompressionState memoryCompressionStateP,
                  uint64_t sliceCountP, bool blockingP, bool dcFlushP, bool useSLMP, bool guardCommandBufferWithPipeControlP, bool gsba32BitRequiredP,
                  bool requiresCoherencyP, bool lowPriorityP, bool implicitFlushP, bool outOfOrderExecutionAllowedP, bool epilogueRequiredP,
                  bool usePerDSSbackedBufferP, bool useSingleSubdeviceP, bool useGlobalAtomicsP, bool areMultipleSubDevicesInContextP, bool memoryMigrationRequiredP, bool textureCacheFlush) : csrDependencies(csrDependenciesP),
                                                                                                                                                                                                barrierTimestampPacketNodes(barrierTimestampPacketNodesP),
                                                                                                                                                                                                pipelineSelectArgs(pipelineSelectArgsP),
                                                                                                                                                                                                flushStampReference(flushStampReferenceP),
                                                                                                                                                                                                throttle(throttleP),
                                                                                                                                                                                                preemptionMode(preemptionModeP),
                                                                                                                                                                                                numGrfRequired(numGrfRequiredP),
                                                                                                                                                                                                l3CacheSettings(l3CacheSettingsP),
                                                                                                                                                                                                threadArbitrationPolicy(threadArbitrationPolicyP),
                                                                                                                                                                                                additionalKernelExecInfo(additionalKernelExecInfoP),
                                                                                                                                                                                                kernelExecutionType(kernelExecutionTypeP),
                                                                                                                                                                                                memoryCompressionState(memoryCompressionStateP),
                                                                                                                                                                                                sliceCount(sliceCountP),
                                                                                                                                                                                                blocking(blockingP),
                                                                                                                                                                                                dcFlush(dcFlushP),
                                                                                                                                                                                                useSLM(useSLMP),
                                                                                                                                                                                                guardCommandBufferWithPipeControl(guardCommandBufferWithPipeControlP),
                                                                                                                                                                                                gsba32BitRequired(gsba32BitRequiredP),
                                                                                                                                                                                                requiresCoherency(requiresCoherencyP),
                                                                                                                                                                                                lowPriority(lowPriorityP),
                                                                                                                                                                                                implicitFlush(implicitFlushP),
                                                                                                                                                                                                outOfOrderExecutionAllowed(outOfOrderExecutionAllowedP),
                                                                                                                                                                                                epilogueRequired(epilogueRequiredP),
                                                                                                                                                                                                usePerDssBackedBuffer(usePerDSSbackedBufferP),
                                                                                                                                                                                                useSingleSubdevice(useSingleSubdeviceP),
                                                                                                                                                                                                useGlobalAtomics(useGlobalAtomicsP),
                                                                                                                                                                                                areMultipleSubDevicesInContext(areMultipleSubDevicesInContextP),
                                                                                                                                                                                                memoryMigrationRequired(memoryMigrationRequiredP),
                                                                                                                                                                                                textureCacheFlush(textureCacheFlush){};

    CsrDependencies csrDependencies;
    TimestampPacketContainer *barrierTimestampPacketNodes = nullptr;
    PipelineSelectArgs pipelineSelectArgs;
    FlushStampTrackingObj *flushStampReference = nullptr;
    QueueThrottle throttle = QueueThrottle::MEDIUM;
    PreemptionMode preemptionMode = PreemptionMode::Disabled;
    uint32_t numGrfRequired = GrfConfig::DefaultGrfNumber;
    uint32_t l3CacheSettings = L3CachingSettings::l3CacheOn;
    int32_t threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
    uint32_t additionalKernelExecInfo = AdditionalKernelExecInfo::NotApplicable;
    KernelExecutionType kernelExecutionType = KernelExecutionType::NotApplicable;
    MemoryCompressionState memoryCompressionState = MemoryCompressionState::NotApplicable;
    uint64_t sliceCount = QueueSliceCount::defaultSliceCount;
    uint64_t engineHints = 0;
    bool blocking = false;
    bool dcFlush = false;
    bool useSLM = false;
    bool guardCommandBufferWithPipeControl = false;
    bool gsba32BitRequired = false;
    bool requiresCoherency = false;
    bool lowPriority = false;
    bool implicitFlush = false;
    bool outOfOrderExecutionAllowed = false;
    bool epilogueRequired = false;
    bool usePerDssBackedBuffer = false;
    bool useSingleSubdevice = false;
    bool useGlobalAtomics = false;
    bool areMultipleSubDevicesInContext = false;
    bool memoryMigrationRequired = false;
    bool textureCacheFlush = false;
    bool disableEUFusion = false;
};

struct CsrSizeRequestFlags {
    bool l3ConfigChanged = false;
    bool preemptionRequestChanged = false;
    bool mediaSamplerConfigChanged = false;
    bool hasSharedHandles = false;
    bool specialPipelineSelectModeChanged = false;
    bool activePartitionsChanged = false;
};
} // namespace NEO
