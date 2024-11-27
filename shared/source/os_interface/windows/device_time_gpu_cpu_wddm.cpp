/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/device_time_wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {
TimeQueryStatus DeviceTimeWddm::getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    TimeQueryStatus retVal = TimeQueryStatus::deviceLost;

    pGpuCpuTime->cpuTimeinNS = 0;
    pGpuCpuTime->gpuTimeStamp = 0;

    TimeStampDataHeader escapeInfo = {};

    if (runEscape(wddm, escapeInfo)) {
        auto &gfxCoreHelper = wddm->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
        convertTimestampsFromOaToCsDomain(gfxCoreHelper, escapeInfo.data.out.gpuPerfTicks, escapeInfo.data.out.gpuPerfFreq, static_cast<uint64_t>(wddm->getTimestampFrequency()));
        double cpuNanoseconds = escapeInfo.data.out.cpuPerfTicks *
                                (1000000000.0 / escapeInfo.data.out.cpuPerfFreq);

        pGpuCpuTime->cpuTimeinNS = (unsigned long long)cpuNanoseconds;
        pGpuCpuTime->gpuTimeStamp = (unsigned long long)escapeInfo.data.out.gpuPerfTicks;
        retVal = TimeQueryStatus::success;
    }

    return retVal;
}
} // namespace NEO
