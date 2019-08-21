/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/csr_definitions.h"

using namespace NEO;

struct DispatchFlagsHelper {
    static DispatchFlags createDefaultDispatchFlags() {
        return DispatchFlags(
            {},                                 //csrDependencies
            {},                                 //pipelineSelectArgs
            nullptr,                            //flushStampReference
            QueueThrottle::MEDIUM,              //throttle
            PreemptionMode::Disabled,           //preemptionMode
            GrfConfig::DefaultGrfNumber,        //numGrfRequired
            L3CachingSettings::l3CacheOn,       //l3CacheSettings
            QueueSliceCount::defaultSliceCount, //sliceCount
            false,                              //blocking
            false,                              //dcFlush
            false,                              //useSLM
            false,                              //guardCommandBufferWithPipeControl
            false,                              //gsba32BitRequired
            false,                              //requiresCoherency
            false,                              //lowPriority
            false,                              //implicitFlush
            false,                              //outOfOrderExecutionAllowed
            false,                              //multiEngineQueue
            false                               //epilogueRequired
        );
    }
};