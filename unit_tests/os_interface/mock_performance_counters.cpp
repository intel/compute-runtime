/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_performance_counters.h"

#include "runtime/os_interface/os_interface.h"

namespace NEO {
MockPerformanceCounters::MockPerformanceCounters(OSTime *osTime)
    : PerformanceCounters(osTime) {
    autoSamplingStartFunc = autoSamplingStart;
    autoSamplingStopFunc = autoSamplingStop;
    hwMetricsEnableFunc = hwMetricsEnableFuncPassing;
    getPmRegsCfgFunc = getPmRegsCfgPassing;
    loadPmRegsCfgFunc = loadPmRegsCfgPassing;
    checkPmRegsCfgFunc = checkPmRegsCfgPassing;
    setPmRegsCfgFunc = setPmRegsCfgFuncPassing;
    sendReadRegsCfgFunc = sendReadRegsCfgFuncPassing;
    getPerfCountersQueryDataFunc = getPerfCountersQueryData;
    PerfCounterFlags::resetPerfCountersFlags();
}
void MockPerformanceCounters::initialize(const HardwareInfo *hwInfo) {
    PerfCounterFlags::initalizeCalled++;
}

int PerfCounterFlags::autoSamplingFuncCalled;
int PerfCounterFlags::autoSamplingStarted;
int PerfCounterFlags::autoSamplingStopped;
int PerfCounterFlags::checkPmRegsCfgCalled;
int PerfCounterFlags::escHwMetricsCalled;
int PerfCounterFlags::getPerfCountersQueryDataCalled;
int PerfCounterFlags::getPmRegsCfgCalled;
int PerfCounterFlags::hwMetricsEnableStatus;
int PerfCounterFlags::initalizeCalled;
int PerfCounterFlags::loadPmRegsCfgCalled;
int PerfCounterFlags::setPmRegsCfgCalled;
int PerfCounterFlags::sendReadRegsCfgCalled;

bool hwMetricsEnableFuncFailing(InstrEscCbData cbData, bool enable) {
    PerfCounterFlags::escHwMetricsCalled++;
    PerfCounterFlags::hwMetricsEnableStatus = enable;
    return false;
}
bool hwMetricsEnableFuncPassing(InstrEscCbData cbData, bool enable) {
    PerfCounterFlags::escHwMetricsCalled++;
    PerfCounterFlags::hwMetricsEnableStatus = enable;
    return true;
}
bool autoSamplingStart(InstrEscCbData cbData, void **ppOAInterface) {
    PerfCounterFlags::autoSamplingFuncCalled++;
    PerfCounterFlags::autoSamplingStarted++;
    ppOAInterface[0] = new char[1];
    return true;
}
bool autoSamplingStartFailing(InstrEscCbData cbData, void **ppOAInterface) {
    PerfCounterFlags::autoSamplingFuncCalled++;
    PerfCounterFlags::autoSamplingStarted++;
    ppOAInterface[0] = nullptr;
    return false;
}
bool autoSamplingStop(void **ppOAInterface) {
    PerfCounterFlags::autoSamplingFuncCalled++;
    PerfCounterFlags::autoSamplingStopped++;
    if (ppOAInterface[0]) {
        delete[] static_cast<char *>(ppOAInterface[0]);
        ppOAInterface[0] = nullptr;
        return true;
    }
    return false;
}
bool getPmRegsCfgPassing(InstrEscCbData cbData, uint32_t cfgId, InstrPmRegsCfg *pCfg, InstrAutoSamplingMode *pAutoSampling) {
    PerfCounterFlags::getPmRegsCfgCalled++;
    if (cfgId == 1) {
        pCfg->ReadRegs.RegsCount = 2;
        pCfg->ReadRegs.Reg[0].BitSize = 32;
        pCfg->ReadRegs.Reg[1].BitSize = 64;
    }
    return true;
}
bool getPmRegsCfgFailing(InstrEscCbData cbData, uint32_t cfgId, InstrPmRegsCfg *pCfg, InstrAutoSamplingMode *pAutoSampling) {
    PerfCounterFlags::getPmRegsCfgCalled++;
    return false;
}
bool checkPmRegsCfgPassing(InstrPmRegsCfg *pQueryPmRegsCfg, uint32_t *pLastPmRegsCfgHandle, const void *pASInterface) {
    PerfCounterFlags::checkPmRegsCfgCalled++;
    return true;
}
bool checkPmRegsCfgFailing(InstrPmRegsCfg *pQueryPmRegsCfg, uint32_t *pLastPmRegsCfgHandle, const void *pASInterface) {
    PerfCounterFlags::checkPmRegsCfgCalled++;
    return false;
}
bool loadPmRegsCfgPassing(InstrEscCbData cbData, InstrPmRegsCfg *pCfg, bool hardwareAccess) {
    PerfCounterFlags::loadPmRegsCfgCalled++;
    return true;
}
bool loadPmRegsCfgFailing(InstrEscCbData cbData, InstrPmRegsCfg *pCfg, bool hardwareAccess) {
    PerfCounterFlags::loadPmRegsCfgCalled++;
    return false;
}

bool setPmRegsCfgFuncPassing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pValues) {
    PerfCounterFlags::setPmRegsCfgCalled++;
    return true;
}
bool setPmRegsCfgFuncFailing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pValues) {
    PerfCounterFlags::setPmRegsCfgCalled++;
    return false;
}
bool sendReadRegsCfgFuncPassing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pBitSizes) {
    PerfCounterFlags::sendReadRegsCfgCalled++;
    return true;
}
bool sendReadRegsCfgFuncFailing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pBitSizes) {
    PerfCounterFlags::sendReadRegsCfgCalled++;
    return false;
}

template <typename GTDI_QUERY, typename HwPerfCountersLayout>
void getPerfCountersQueryData(InstrEscCbData cbData, GTDI_QUERY *pData, HwPerfCountersLayout *pLayout, uint64_t cpuRawTimestamp, void *pASInterface, InstrPmRegsCfg *pPmRegsCfg, bool useMiRPC, bool resetASData, const InstrAllowedContexts *pAllowedContexts) {
    PerfCounterFlags::getPerfCountersQueryDataCalled++;
}

void PerfCounterFlags::resetPerfCountersFlags() {
    PerfCounterFlags::autoSamplingFuncCalled = 0;
    PerfCounterFlags::autoSamplingStarted = 0;
    PerfCounterFlags::autoSamplingStopped = 0;
    PerfCounterFlags::checkPmRegsCfgCalled = 0;
    PerfCounterFlags::escHwMetricsCalled = 0;
    PerfCounterFlags::getPerfCountersQueryDataCalled = 0;
    PerfCounterFlags::getPmRegsCfgCalled = 0;
    PerfCounterFlags::hwMetricsEnableStatus = 0;
    PerfCounterFlags::initalizeCalled = 0;
    PerfCounterFlags::loadPmRegsCfgCalled = 0;
    PerfCounterFlags::setPmRegsCfgCalled = 0;
    PerfCounterFlags::sendReadRegsCfgCalled = 0;
}
void PerformanceCountersFixture::SetUp() {
    osInterfaceBase = std::unique_ptr<OSInterface>(new OSInterface());
    createOsTime();
    fillOsInterface();
    PerfCounterFlags::resetPerfCountersFlags();
}
} // namespace NEO
