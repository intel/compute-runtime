/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "performance_counters_linux.h"

namespace NEO {

std::unique_ptr<PerformanceCounters> PerformanceCounters::create(OSTime *osTime) {
    return std::unique_ptr<PerformanceCounters>(new PerformanceCountersLinux(osTime));
}
PerformanceCountersLinux::PerformanceCountersLinux(OSTime *osTime) : PerformanceCounters(osTime) {
    mdLibHandle = nullptr;
    perfmonLoadConfigFunc = nullptr;
}

PerformanceCountersLinux::~PerformanceCountersLinux() {
    if (pAutoSamplingInterface) {
        autoSamplingStopFunc(&pAutoSamplingInterface);
        pAutoSamplingInterface = nullptr;
        available = false;
    }

    if (mdLibHandle) {
        dlcloseFunc(mdLibHandle);
        mdLibHandle = nullptr;
    }
}

void PerformanceCountersLinux::initialize(const HardwareInfo *hwInfo) {
    PerformanceCounters::initialize(hwInfo);
    mdLibHandle = dlopenFunc("libmd.so", RTLD_LAZY | RTLD_LOCAL);
    if (mdLibHandle) {
        perfmonLoadConfigFunc = reinterpret_cast<perfmonLoadConfig_t>(dlsymFunc(mdLibHandle, "drm_intel_perfmon_load_config"));
    }
    setPlatformInfoFunc(hwInfo->pPlatform->eProductFamily, (void *)(hwInfo->pSkuTable));
}

void PerformanceCountersLinux::enableImpl() {
    if (mdLibHandle && perfmonLoadConfigFunc) {
        PerformanceCounters::enableImpl();
    }
}

bool PerformanceCountersLinux::verifyPmRegsCfg(InstrPmRegsCfg *pCfg, uint32_t *pLastPmRegsCfgHandle, bool *pLastPmRegsCfgPending) {
    if (perfmonLoadConfigFunc == nullptr) {
        return false;
    }
    if (PerformanceCounters::verifyPmRegsCfg(pCfg, pLastPmRegsCfgHandle, pLastPmRegsCfgPending)) {
        return getPerfmonConfig(pCfg);
    }
    return false;
}
bool PerformanceCountersLinux::getPerfmonConfig(InstrPmRegsCfg *pCfg) {
    unsigned int oaCfgHandle = pCfg->OaCounters.Handle;
    unsigned int gpCfgHandle = pCfg->GpCounters.Handle;
    int fd = osInterface->get()->getDrm()->getFileDescriptor();
    if (perfmonLoadConfigFunc(fd, nullptr, &oaCfgHandle, &gpCfgHandle) != 0) {
        return false;
    }
    if (pCfg->OaCounters.Handle != 0 && oaCfgHandle != pCfg->OaCounters.Handle) {
        return false;
    }
    if (pCfg->GpCounters.Handle != 0 && gpCfgHandle != pCfg->GpCounters.Handle) {
        return false;
    }
    return true;
}
} // namespace NEO
