/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/kernel/grf_config.h"
#include <limits>

namespace OCLRT {
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

struct DispatchFlags {
    bool blocking = false;
    bool dcFlush = false;
    bool useSLM = false;
    bool guardCommandBufferWithPipeControl = false;
    bool GSBA32BitRequired = false;
    bool mediaSamplerRequired = false;
    bool requiresCoherency = false;
    bool lowPriority = false;
    QueueThrottle throttle = QueueThrottle::MEDIUM;
    bool implicitFlush = false;
    bool outOfOrderExecutionAllowed = false;
    FlushStampTrackingObj *flushStampReference = nullptr;
    PreemptionMode preemptionMode = PreemptionMode::Disabled;
    EventsRequest *outOfDeviceDependencies = nullptr;
    uint32_t numGrfRequired = GrfConfig::DefaultGrfNumber;
    bool specialPipelineSelectMode = false;
};

struct CsrSizeRequestFlags {
    bool l3ConfigChanged = false;
    bool coherencyRequestChanged = false;
    bool preemptionRequestChanged = false;
    bool mediaSamplerConfigChanged = false;
    bool hasSharedHandles = false;
    bool numGrfRequiredChanged = false;
    bool specialPipelineSelectModeChanged = false;
};
} // namespace OCLRT
