/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/command_stream/csr_definitions.h"

using namespace NEO;

struct DispatchFlagsHelper {
    static DispatchFlags createDefaultDispatchFlags() {
        return DispatchFlags(
            {},                                  //csrDependencies
            nullptr,                             //barrierTimestampPacketNodes
            {},                                  //pipelineSelectArgs
            nullptr,                             //flushStampReference
            QueueThrottle::MEDIUM,               //throttle
            PreemptionMode::Disabled,            //preemptionMode
            GrfConfig::DefaultGrfNumber,         //numGrfRequired
            L3CachingSettings::l3CacheOn,        //l3CacheSettings
            ThreadArbitrationPolicy::NotPresent, //threadArbitrationPolicy
            QueueSliceCount::defaultSliceCount,  //sliceCount
            false,                               //blocking
            false,                               //dcFlush
            false,                               //useSLM
            false,                               //guardCommandBufferWithPipeControl
            false,                               //gsba32BitRequired
            false,                               //requiresCoherency
            false,                               //lowPriority
            false,                               //implicitFlush
            false,                               //outOfOrderExecutionAllowed
            false                                //epilogueRequired
        );
    }
};
