/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_time_win.h"

#include "shared/source/os_interface/windows/device_time_wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include <memory>

#undef WIN32_NO_STATUS

namespace NEO {

bool OSTimeWin::getCpuTime(uint64_t *timeStamp) {
    uint64_t time;
    this->QueryPerfomanceCounterFnc((LARGE_INTEGER *)&time);

    *timeStamp = static_cast<uint64_t>((static_cast<double>(time) * NSEC_PER_SEC / frequency.QuadPart));
    return true;
};

std::unique_ptr<OSTime> OSTimeWin::create(OSInterface *osInterface) {
    return std::unique_ptr<OSTime>(new OSTimeWin(osInterface));
}

OSTimeWin::OSTimeWin(OSInterface *osInterface) {
    this->osInterface = osInterface;
    Wddm *wddm = osInterface ? osInterface->getDriverModel()->as<Wddm>() : nullptr;
    this->deviceTime = std::make_unique<DeviceTimeWddm>(wddm);
    QueryPerformanceFrequency(&frequency);
}

double OSTimeWin::getHostTimerResolution() const {
    double retValue = 0;
    if (frequency.QuadPart) {
        retValue = 1e9 / frequency.QuadPart;
    }
    return retValue;
}

uint64_t OSTimeWin::getCpuRawTimestamp() {
    LARGE_INTEGER cpuRawTimestamp = {};
    this->QueryPerfomanceCounterFnc(&cpuRawTimestamp);
    return cpuRawTimestamp.QuadPart;
}
} // namespace NEO
