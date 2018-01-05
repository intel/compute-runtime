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

#include "performance_counters_linux.h"

namespace OCLRT {

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
} // namespace OCLRT
