/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/event_profiling_helpers.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_time.h"

namespace NEO {

uint64_t restoreHighBitsFromReference(uint64_t rawValue, uint64_t reference, uint32_t globalTimeStampBits) {
    DEBUG_BREAK_IF(globalTimeStampBits >= 64);
    return rawValue | (reference & (maxNBitValue(64) - maxNBitValue(globalTimeStampBits)));
}

uint64_t wrapAwareDelta(uint64_t startTime, uint64_t endTime, uint32_t validBits) {
    uint64_t max = maxNBitValue(validBits);
    uint64_t delta = 0;

    startTime &= max;
    endTime &= max;

    if (startTime > endTime) {
        delta = max - startTime;
        delta += endTime;
    } else {
        delta = endTime - startTime;
    }

    return delta;
}

void updateProfilingTimestamp(ProfilingInfo &timestamp, uint64_t newGpuTimeStamp, const GfxCoreHelper &gfxCoreHelper, double resolution) {
    timestamp.gpuTimeStamp = newGpuTimeStamp;
    timestamp.gpuTimeInNs = gfxCoreHelper.getGpuTimeStampInNS(timestamp.gpuTimeStamp, resolution);
    timestamp.cpuTimeInNs = timestamp.gpuTimeInNs;
}

void calculateProfilingData(const GfxCoreHelper &gfxCoreHelper, OSTime &osTime, double resolution, uint32_t kernelTimestampValidBits,
                            ProfilingInfo &queueTimeStamp, ProfilingInfo &submitTimeStamp, ProfilingInfo &startTimeStamp,
                            ProfilingInfo &endTimeStamp, ProfilingInfo &completeTimeStamp,
                            uint64_t contextStartTS, uint64_t contextEndTS, uint64_t *contextCompleteTS, uint64_t globalStartTS) {
    // Calculate startTimestamp only if it was not already set on CPU
    if (startTimeStamp.cpuTimeInNs == 0) {
        startTimeStamp.gpuTimeStamp = restoreHighBitsFromReference(globalStartTS, submitTimeStamp.gpuTimeStamp, gfxCoreHelper.getGlobalTimeStampBits());
        if (startTimeStamp.gpuTimeStamp < submitTimeStamp.gpuTimeStamp) {
            auto diff = submitTimeStamp.gpuTimeStamp - startTimeStamp.gpuTimeStamp;
            auto diffInNS = gfxCoreHelper.getGpuTimeStampInNS(diff, resolution);
            if (diffInNS < osTime.getTimestampRefreshTimeout()) {
                auto alignedSubmitTimestamp = startTimeStamp.gpuTimeStamp - 1;
                auto alignedQueueTimestamp = startTimeStamp.gpuTimeStamp - 2;
                if (startTimeStamp.gpuTimeStamp <= 2) {
                    alignedSubmitTimestamp = 0;
                    alignedQueueTimestamp = 0;
                }
                updateProfilingTimestamp(submitTimeStamp, alignedSubmitTimestamp, gfxCoreHelper, resolution);
                updateProfilingTimestamp(queueTimeStamp, alignedQueueTimestamp, gfxCoreHelper, resolution);
                osTime.setRefreshTimestampsFlag();
            } else {
                startTimeStamp.gpuTimeStamp += static_cast<uint64_t>(1ULL << gfxCoreHelper.getGlobalTimeStampBits());
            }
        }
    }

    UNRECOVERABLE_IF(startTimeStamp.gpuTimeStamp < submitTimeStamp.gpuTimeStamp);
    auto gpuTicksDiff = startTimeStamp.gpuTimeStamp - submitTimeStamp.gpuTimeStamp;
    auto timeDiff = static_cast<uint64_t>(gpuTicksDiff * resolution);
    startTimeStamp.cpuTimeInNs = submitTimeStamp.cpuTimeInNs + timeDiff;
    startTimeStamp.gpuTimeInNs = gfxCoreHelper.getGpuTimeStampInNS(startTimeStamp.gpuTimeStamp, resolution);

    // If device enqueue has not updated complete timestamp, assign end timestamp
    uint64_t gpuDuration = 0;
    uint64_t cpuDuration = 0;

    uint64_t gpuCompleteDuration = 0;
    uint64_t cpuCompleteDuration = 0;

    gpuDuration = wrapAwareDelta(contextStartTS, contextEndTS, kernelTimestampValidBits);
    if (*contextCompleteTS == 0) {
        *contextCompleteTS = contextEndTS;
        gpuCompleteDuration = gpuDuration;
    } else {
        gpuCompleteDuration = wrapAwareDelta(contextStartTS, *contextCompleteTS, kernelTimestampValidBits);
    }
    cpuDuration = static_cast<uint64_t>(gpuDuration * resolution);
    cpuCompleteDuration = static_cast<uint64_t>(gpuCompleteDuration * resolution);

    endTimeStamp.cpuTimeInNs = startTimeStamp.cpuTimeInNs + cpuDuration;
    endTimeStamp.gpuTimeInNs = startTimeStamp.gpuTimeInNs + cpuDuration;
    endTimeStamp.gpuTimeStamp = startTimeStamp.gpuTimeStamp + gpuDuration;

    completeTimeStamp.cpuTimeInNs = startTimeStamp.cpuTimeInNs + cpuCompleteDuration;
    completeTimeStamp.gpuTimeInNs = startTimeStamp.gpuTimeInNs + cpuCompleteDuration;
    completeTimeStamp.gpuTimeStamp = startTimeStamp.gpuTimeStamp + gpuCompleteDuration;

    if (debugManager.flags.ReturnRawGpuTimestamps.get()) {
        startTimeStamp.gpuTimeStamp = contextStartTS;
        endTimeStamp.gpuTimeStamp = contextEndTS;
        completeTimeStamp.gpuTimeStamp = *contextCompleteTS;
    }
}

} // namespace NEO
