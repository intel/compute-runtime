/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/profiling_info.h"

#include <cstdint>

namespace NEO {
class GfxCoreHelper;
class OSTime;

// Restores high overflow bits a narrow packet dropped, taking them from a wider reference (the submit stamp).
uint64_t restoreHighBitsFromReference(uint64_t rawValue, uint64_t reference, uint32_t globalTimeStampBits);

// Wrap-aware delta over a counter of the given valid width (single wrap handled).
uint64_t wrapAwareDelta(uint64_t startTime, uint64_t endTime, uint32_t validBits);

void updateProfilingTimestamp(ProfilingInfo &timestamp, uint64_t newGpuTimeStamp, const GfxCoreHelper &gfxCoreHelper, double resolution);

// Rebases raw kernel start/end onto the submit epoch and fills start/end/complete (cpu ns, gpu ns, gpu ticks).
void calculateProfilingData(const GfxCoreHelper &gfxCoreHelper, OSTime &osTime, double resolution, uint32_t kernelTimestampValidBits,
                            ProfilingInfo &queueTimeStamp, ProfilingInfo &submitTimeStamp, ProfilingInfo &startTimeStamp,
                            ProfilingInfo &endTimeStamp, ProfilingInfo &completeTimeStamp,
                            uint64_t contextStartTS, uint64_t contextEndTS, uint64_t *contextCompleteTS, uint64_t globalStartTS);

} // namespace NEO
