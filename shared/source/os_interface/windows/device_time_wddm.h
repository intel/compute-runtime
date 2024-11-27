/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/windows/gfx_escape_wrapper.h"

#define GFX_ESCAPE_IGPA_INSTRUMENTATION_CONTROL 12

namespace NEO {
class Wddm;
class GfxCoreHelper;
struct TimeStampDataHeader;

class DeviceTimeWddm : public DeviceTime {
  public:
    DeviceTimeWddm(Wddm *wddm);
    TimeQueryStatus getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    double getDynamicDeviceTimerResolution() const override;
    uint64_t getDynamicDeviceTimerClock() const override;

  protected:
    MOCKABLE_VIRTUAL bool runEscape(Wddm *wddm, TimeStampDataHeader &escapeInfo);
    void convertTimestampsFromOaToCsDomain(const GfxCoreHelper &gfxCoreHelper, uint64_t &timestampData, uint64_t freqOA, uint64_t freqCS);
    Wddm *wddm = nullptr;
};

struct GetGpuCpuTimestampsIn {
    int function = 25; // GTDI_FNC_GET_GPU_CPU_TIMESTAMPS
};

struct GetGpuCpuTimestampsOut {
    int retCode;           // Result of the call
    uint64_t gpuPerfTicks; // in GPU_timestamp_ticks
    uint64_t cpuPerfTicks; // in CPU_timestamp_ticks
    uint64_t gpuPerfFreq;  // in GPU_timestamp_ticks/s
    uint64_t cpuPerfFreq;  // in CPU_timestamp_ticks/s
};

struct TimeStampDataHeader {
    GFX_ESCAPE_HEADER_T header;
    union {
        GetGpuCpuTimestampsIn in;
        GetGpuCpuTimestampsOut out;
    } data;
};

} // namespace NEO
