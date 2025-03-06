/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/csr_properties_flags.h"

using namespace NEO;

struct DispatchFlagsHelper {
    static DispatchFlags createDefaultDispatchFlags() {
        return DispatchFlags(
            nullptr,                                 // barrierTimestampPacketNodes
            {},                                      // pipelineSelectArgs
            nullptr,                                 // flushStampReference
            QueueThrottle::MEDIUM,                   // throttle
            PreemptionMode::Disabled,                // preemptionMode
            GrfConfig::defaultGrfNumber,             // numGrfRequired
            L3CachingSettings::l3CacheOn,            // l3CacheSettings
            ThreadArbitrationPolicy::NotPresent,     // threadArbitrationPolicy
            AdditionalKernelExecInfo::notApplicable, // additionalKernelExecInfo
            KernelExecutionType::notApplicable,      // kernelExecutionType
            MemoryCompressionState::notApplicable,   // memoryCompressionState
            QueueSliceCount::defaultSliceCount,      // sliceCount
            false,                                   // blocking
            false,                                   // dcFlush
            false,                                   // useSLM
            false,                                   // guardCommandBufferWithPipeControl
            false,                                   // gsba32BitRequired
            false,                                   // lowPriority
            false,                                   // implicitFlush
            false,                                   // outOfOrderExecutionAllowed
            false,                                   // epilogueRequired
            false,                                   // usePerDssBackedBuffer
            false,                                   // areMultipleSubDevicesInContext
            false,                                   // memoryMigrationRequired
            false,                                   // textureCacheFlush
            false,                                   // hasStallingCmds
            false,                                   // hasRelaxedOrderingDependencies
            false,                                   // stateCacheInvalidation
            false,                                   // isStallingCommandsOnNextFlushRequired
            false                                    // isDcFlushRequiredOnStallingCommandsOnNextFlush
        );
    }

    static DispatchBcsFlags createDefaultBcsDispatchFlags() {
        return DispatchBcsFlags(false, false, false);
    }
};
