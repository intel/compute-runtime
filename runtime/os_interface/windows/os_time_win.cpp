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

#include <memory>
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/os_time_win.h"

#undef WIN32_NO_STATUS

namespace OCLRT {

bool runEscape(Wddm *wddm, TimeStampDataHeader &escapeInfo) {
    if (wddm) {
        D3DKMT_ESCAPE escapeCommand = {0};

        GTDIGetGpuCpuTimestampsIn in = {GTDI_FNC_GET_GPU_CPU_TIMESTAMPS};
        GTDIGetGpuCpuTimestampsOut out = {GTDI_RET_FAILED, 0};
        uint32_t inSize = sizeof(GTDIGetGpuCpuTimestampsIn);
        uint32_t outSize = sizeof(GTDIGetGpuCpuTimestampsOut);

        escapeInfo.m_Header.EscapeCode = GFX_ESCAPE_IGPA_INSTRUMENTATION_CONTROL;
        escapeInfo.m_Header.Size = outSize;
        escapeInfo.m_Data.m_In = in;

        escapeCommand.Flags.Value = 0;
        escapeCommand.hAdapter = (D3DKMT_HANDLE)0;
        escapeCommand.hContext = (D3DKMT_HANDLE)0; // escape is not context specific
        escapeCommand.hDevice = (D3DKMT_HANDLE)0;  // escape not device specific
        escapeCommand.pPrivateDriverData = &escapeInfo;
        escapeCommand.PrivateDriverDataSize = sizeof(escapeInfo);
        escapeCommand.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;

        auto status = wddm->escape(escapeCommand);

        if (status == STATUS_SUCCESS) {
            return true;
        }
    }

    return false;
}

bool OSTimeWin::getCpuGpuTime(TimeStampData *pGpuCpuTime) {
    bool retVal = false;

    pGpuCpuTime->CPUTimeinNS = 0;
    pGpuCpuTime->GPUTimeStamp = 0;

    TimeStampDataHeader escapeInfo = {0};

    if (runEscape(wddm, escapeInfo)) {
        double cpuNanoseconds = escapeInfo.m_Data.m_Out.cpuPerfTicks *
                                (1000000000.0 / escapeInfo.m_Data.m_Out.cpuPerfFreq);

        pGpuCpuTime->CPUTimeinNS = (unsigned long long)cpuNanoseconds;
        pGpuCpuTime->GPUTimeStamp = (unsigned long long)escapeInfo.m_Data.m_Out.gpuPerfTicks;
        retVal = true;
    }

    return retVal;
}

bool OSTimeWin::getCpuTime(uint64_t *timeStamp) {
    uint64_t time;
    this->QueryPerfomanceCounterFnc((LARGE_INTEGER *)&time);

    *timeStamp = static_cast<uint64_t>((static_cast<double>(time) * NSEC_PER_SEC / frequency.QuadPart));
    return true;
};

std::unique_ptr<OSTime> OSTime::create(OSInterface *osInterface) {
    return std::unique_ptr<OSTime>(new OSTimeWin(osInterface));
}

OSTimeWin::OSTimeWin(OSInterface *osInterface) : wddm(nullptr) {
    this->osInterface = osInterface;
    if (osInterface) {
        wddm = osInterface->get()->getWddm();
    }
    QueryPerformanceFrequency(&frequency);
}

double OSTimeWin::getHostTimerResolution() const {
    double retValue = 0;
    if (frequency.QuadPart) {
        retValue = 1e9 / frequency.QuadPart;
    }
    return retValue;
}

double OSTimeWin::getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
    double retVal = 0;
    TimeStampDataHeader escapeInfo = {0};

    if (runEscape(wddm, escapeInfo)) {
        retVal = 1000000000.0 / (double)escapeInfo.m_Data.m_Out.gpuPerfFreq;
    }

    return retVal;
}

uint64_t OSTimeWin::getCpuRawTimestamp() {
    LARGE_INTEGER cpuRawTimestamp = {};
    this->QueryPerfomanceCounterFnc(&cpuRawTimestamp);
    return cpuRawTimestamp.QuadPart;
}
} // namespace OCLRT
