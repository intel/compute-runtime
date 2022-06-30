/*
 * Copyright (C) 2019-2021 Intel Corporation
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
            {},                                      //csrDependencies
            nullptr,                                 //barrierTimestampPacketNodes
            {},                                      //pipelineSelectArgs
            nullptr,                                 //flushStampReference
            QueueThrottle::MEDIUM,                   //throttle
            PreemptionMode::Disabled,                //preemptionMode
            GrfConfig::DefaultGrfNumber,             //numGrfRequired
            L3CachingSettings::l3CacheOn,            //l3CacheSettings
            ThreadArbitrationPolicy::NotPresent,     //threadArbitrationPolicy
            AdditionalKernelExecInfo::NotApplicable, //additionalKernelExecInfo
            KernelExecutionType::NotApplicable,      //kernelExecutionType
            MemoryCompressionState::NotApplicable,   //memoryCompressionState
            QueueSliceCount::defaultSliceCount,      //sliceCount
            false,                                   //blocking
            false,                                   //dcFlush
            false,                                   //useSLM
            false,                                   //guardCommandBufferWithPipeControl
            false,                                   //gsba32BitRequired
            false,                                   //requiresCoherency
            false,                                   //lowPriority
            false,                                   //implicitFlush
            false,                                   //outOfOrderExecutionAllowed
            false,                                   //epilogueRequired
            false,                                   //usePerDssBackedBuffer
            false,                                   //useSingleSubdevice
            false,                                   //useGlobalAtomics
            false,                                   //areMultipleSubDevicesInContext
            false,                                   //memoryMigrationRequired
            false                                    //textureCacheFlush
        );
    }
};
