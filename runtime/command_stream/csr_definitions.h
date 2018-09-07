/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/properties_helper.h"
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
};

struct CsrSizeRequestFlags {
    bool l3ConfigChanged = false;
    bool coherencyRequestChanged = false;
    bool preemptionRequestChanged = false;
    bool mediaSamplerConfigChanged = false;
    bool hasSharedHandles = false;
};
} // namespace OCLRT
