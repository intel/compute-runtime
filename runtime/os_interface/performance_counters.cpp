/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "CL/cl.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/performance_counters.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/os_interface/os_time.h"

namespace OCLRT {
decltype(&instrGetPerfCountersQueryData) getPerfCountersQueryDataFactory[IGFX_MAX_CORE] = {
    nullptr,
};
size_t perfCountersQuerySize[IGFX_MAX_CORE] = {
    0,
};

PerformanceCounters::PerformanceCounters(OSTime *osTime) {
    this->osTime = osTime;
    DEBUG_BREAK_IF(osTime == nullptr);
    gfxFamily = IGFX_UNKNOWN_CORE;
    cbData = {
        0,
    };
    this->osInterface = osTime->getOSInterface();
    hwMetricsEnabled = false;
    useMIRPC = false;
    pAutoSamplingInterface = nullptr;
    cpuRawTimestamp = 0;
    refCounter = 0;
    available = false;
    reportId = 0;
}

void PerformanceCounters::enable() {
    mutex.lock();
    std::lock_guard<std::mutex> lg(mutex, std::adopt_lock);
    if (refCounter == 0) {
        enableImpl();
    }
    refCounter++;
}
void PerformanceCounters::shutdown() {
    mutex.lock();
    std::lock_guard<std::mutex> lg(mutex, std::adopt_lock);
    if (refCounter >= 1) {
        if (refCounter == 1) {
            shutdownImpl();
        }
        refCounter--;
    }
}

void PerformanceCounters::initialize(const HardwareInfo *hwInfo) {
    useMIRPC = !(hwInfo->pWaTable->waDoNotUseMIReportPerfCount);
    gfxFamily = hwInfo->pPlatform->eRenderCoreFamily;

    if (getPerfCountersQueryDataFactory[gfxFamily] != nullptr) {
        getPerfCountersQueryDataFunc = getPerfCountersQueryDataFactory[gfxFamily];
    } else {
        perfCountersQuerySize[gfxFamily] = sizeof(GTDI_QUERY);
    }
}
void PerformanceCounters::enableImpl() {
    hwMetricsEnabled = hwMetricsEnableFunc(cbData, true);

    if (!pAutoSamplingInterface && hwMetricsEnabled) {
        autoSamplingStartFunc(cbData, &pAutoSamplingInterface);
        if (pAutoSamplingInterface) {
            available = true;
        }
    }
}
void PerformanceCounters::shutdownImpl() {
    if (hwMetricsEnabled) {
        hwMetricsEnableFunc(cbData, false);
        hwMetricsEnabled = false;
    }
    if (pAutoSamplingInterface) {
        autoSamplingStopFunc(&pAutoSamplingInterface);
        pAutoSamplingInterface = nullptr;
        available = false;
    }
}
void PerformanceCounters::setCpuTimestamp() {
    cpuRawTimestamp = osTime->getCpuRawTimestamp();
}
InstrPmRegsCfg *PerformanceCounters::getPmRegsCfg(uint32_t configuration) {
    if (!hwMetricsEnabled) {
        return nullptr;
    }

    switch (configuration) {
    case GTDI_CONFIGURATION_SET_DYNAMIC:
    case GTDI_CONFIGURATION_SET_1:
    case GTDI_CONFIGURATION_SET_2:
    case GTDI_CONFIGURATION_SET_3:
        break;

    default:
        return nullptr;
    }

    InstrPmRegsCfg *pPmRegsCfg = new InstrPmRegsCfg();
    pPmRegsCfg->OaCounters.Handle = INSTR_PM_REGS_CFG_INVALID;

    mutex.lock();
    std::lock_guard<std::mutex> lg(mutex, std::adopt_lock);

    if (getPmRegsCfgFunc(cbData, configuration, pPmRegsCfg, nullptr)) {
        return pPmRegsCfg;
    }
    delete pPmRegsCfg;
    return nullptr;
}
bool PerformanceCounters::verifyPmRegsCfg(InstrPmRegsCfg *pCfg, uint32_t *pLastPmRegsCfgHandle, bool *pLastPmRegsCfgPending) {
    if (pCfg == nullptr || pLastPmRegsCfgHandle == nullptr || pLastPmRegsCfgPending == nullptr) {
        return false;
    }
    if (checkPmRegsCfgFunc(pCfg, pLastPmRegsCfgHandle, pAutoSamplingInterface)) {
        if (loadPmRegsCfgFunc(cbData, pCfg, 1)) {
            return true;
        }
    }
    return false;
}
bool PerformanceCounters::sendPmRegsCfgCommands(InstrPmRegsCfg *pCfg, uint32_t *pLastPmRegsCfgHandle, bool *pLastPmRegsCfgPending) {
    if (verifyPmRegsCfg(pCfg, pLastPmRegsCfgHandle, pLastPmRegsCfgPending)) {
        *pLastPmRegsCfgPending = true;
        return true;
    }
    return false;
}
bool PerformanceCounters::processEventReport(size_t inputParamSize, void *inputParam, size_t *outputParamSize, HwPerfCounter *pPrivateData, InstrPmRegsCfg *countersConfiguration, bool isEventComplete) {
    size_t outputSize = perfCountersQuerySize[gfxFamily];
    if (outputParamSize) {
        *outputParamSize = outputSize;
    }
    if (inputParam == nullptr && inputParamSize == 0 && outputParamSize) {
        return true;
    }
    if (inputParam == nullptr || isEventComplete == false) {
        return false;
    }
    if (inputParamSize < outputSize) {
        return false;
    }
    GTDI_QUERY *pClientData = static_cast<GTDI_QUERY *>(inputParam);
    getPerfCountersQueryDataFunc(cbData, pClientData, &pPrivateData->HWPerfCounters,
                                 cpuRawTimestamp, pAutoSamplingInterface, countersConfiguration, useMIRPC, true, nullptr);
    return true;
}

int PerformanceCounters::sendPerfConfiguration(uint32_t count, uint32_t *pOffsets, uint32_t *pValues) {
    bool ret = false;

    if (count == 0 || pOffsets == NULL || pValues == NULL) {
        return CL_INVALID_VALUE;
    }

    mutex.lock();
    std::lock_guard<std::mutex> lg(mutex, std::adopt_lock);
    if (pOffsets[0] != INSTR_READ_REGS_CFG_TAG) {
        ret = setPmRegsCfgFunc(cbData, count, pOffsets, pValues);
    } else if (count > 1) {
        ret = sendReadRegsCfgFunc(cbData, count - 1, pOffsets + 1, pValues + 1);
    }

    return ret ? CL_SUCCESS : CL_PROFILING_INFO_NOT_AVAILABLE;
}

uint32_t PerformanceCounters::getCurrentReportId() {
    return (osInterface->getHwContextId() << 12) | getReportId();
}
} // namespace OCLRT
