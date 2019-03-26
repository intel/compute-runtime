/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance_counters_win.h"

#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/windows_wrapper.h"

namespace NEO {
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

} // namespace NEO
