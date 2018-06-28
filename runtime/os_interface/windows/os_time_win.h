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
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "gfxEscape.h"
#include "runtime/os_interface/os_time.h"

namespace OCLRT {
class Wddm;

class OSTimeWin : public OSTime {

  public:
    OSTimeWin(OSInterface *osInterface);
    bool getCpuTime(uint64_t *timeStamp) override;
    bool getCpuGpuTime(TimeStampData *pGpuCpuTime) override;
    double getHostTimerResolution() const override;
    double getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const override;
    uint64_t getCpuRawTimestamp() override;

  protected:
    Wddm *wddm = nullptr;
    LARGE_INTEGER frequency;
    OSTimeWin() = default;
    decltype(&QueryPerformanceCounter) QueryPerfomanceCounterFnc = QueryPerformanceCounter;
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

} // namespace OCLRT
