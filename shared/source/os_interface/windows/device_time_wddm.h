/*
 * Copyright (C) 2017-2021 Intel Corporation
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
struct TimeStampDataHeader;

class DeviceTimeWddm : public DeviceTime {
  public:
    DeviceTimeWddm(Wddm *wddm);
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override;
    uint64_t getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const override;

  protected:
    MOCKABLE_VIRTUAL bool runEscape(Wddm *wddm, TimeStampDataHeader &escapeInfo);
    Wddm *wddm = nullptr;
};

typedef enum GTDI_ESCAPE_FUNCTION_ENUM {
    GTDI_FNC_GET_GPU_CPU_TIMESTAMPS = 25
} GTDI_ESCAPE_FUNCTION;

typedef struct GTDIBaseInStruct {
    GTDI_ESCAPE_FUNCTION Function;
} GTDIHeaderIn;

typedef GTDIHeaderIn GTDIGetGpuCpuTimestampsIn;

typedef enum GTDI_RETURN_CODE_ENUM {
    GTDI_RET_OK = 0,
    GTDI_RET_FAILED,
    GTDI_RET_NOT_CONNECTED,
    GTDI_RET_HW_METRICS_NOT_ENABLED,
    GTDI_RET_CONTEXT_ID_MISMATCH,
    GTDI_RET_NOT_SUPPORTED,
    GTDI_RET_PENDING,
    GTDI_RET_INVALID_CONFIGURATION,
    GTDI_RET_CONCURRENT_API_ENABLED,
    GTDI_RET_NO_INFORMATION, // for GTDI_FNC_GET_ERROR_INFO escape only
    // ...
    GTDI_RET_MAX = 0xFFFFFFFF
} GTDI_RETURN_CODE;

typedef struct GTDIGetGpuCpuTimestampsOutStruct {
    GTDI_RETURN_CODE RetCode; // Result of the call
    uint64_t gpuPerfTicks;    // in GPU_timestamp_ticks
    uint64_t cpuPerfTicks;    // in CPU_timestamp_ticks
    uint64_t gpuPerfFreq;     // in GPU_timestamp_ticks/s
    uint64_t cpuPerfFreq;     // in CPU_timestamp_ticks/s
} GTDIGetGpuCpuTimestampsOut;

struct TimeStampDataHeader {
    GFX_ESCAPE_HEADER_T m_Header;
    union {
        GTDIGetGpuCpuTimestampsIn m_In;
        GTDIGetGpuCpuTimestampsOut m_Out;
    } m_Data;
};

} // namespace NEO
