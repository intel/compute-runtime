/*
* Copyright (c) 2017, Intel Corporation
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

#include "config.h"
#include "runtime/os_interface/windows/windows_wrapper.h"
#include "performance_counters_win.h"
#include "runtime/os_interface/windows/os_interface.h"

namespace OCLRT {
std::unique_ptr<PerformanceCounters> PerformanceCounters::create(OSTime *osTime) {
    return std::unique_ptr<PerformanceCounters>(new PerformanceCountersWin(osTime));
}
PerformanceCountersWin::PerformanceCountersWin(OSTime *osTime) : PerformanceCounters(osTime) {
    cbData.hAdapter = (void *)(UINT_PTR)osInterface->get()->getAdapterHandle();
    cbData.hDevice = (void *)(UINT_PTR)osInterface->get()->getDeviceHandle();
    cbData.pfnEscapeCb = osInterface->get()->getEscapeHandle();
}

PerformanceCountersWin::~PerformanceCountersWin() {
    if (pAutoSamplingInterface) {
        autoSamplingStopFunc(&pAutoSamplingInterface);
        pAutoSamplingInterface = nullptr;
        available = false;
    }
}

void PerformanceCountersWin::initialize(const HardwareInfo *hwInfo) {
    PerformanceCounters::initialize(hwInfo);
    setAvailableFunc(true);
    verifyEnableFunc(cbData);
}

} // namespace OCLRT
