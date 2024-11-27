/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_time_win.h"

#include "shared/source/helpers/hw_info.h"
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

bool OSTime::getCpuTimeHost(uint64_t *timeStamp) {
    uint64_t time;
    QueryPerformanceCounter((LARGE_INTEGER *)&time);

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    *timeStamp = static_cast<uint64_t>((static_cast<double>(time) * NSEC_PER_SEC / frequency.QuadPart));
    return false;
};

std::unique_ptr<OSTime> OSTimeWin::create(OSInterface &osInterface) {
    return std::unique_ptr<OSTime>(new OSTimeWin(osInterface));
}

OSTimeWin::OSTimeWin(OSInterface &osInterface) {
    this->osInterface = &osInterface;
    Wddm *wddm = osInterface.getDriverModel()->as<Wddm>();
    auto hwInfo = wddm->getHardwareInfo();
    if (hwInfo->capabilityTable.timestampValidBits < 64) {
        maxGpuTimeStamp = 1ull << hwInfo->capabilityTable.timestampValidBits;
    }
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
