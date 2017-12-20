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

#include "mock_performance_counters.h"
#include "runtime/os_interface/os_interface.h"

namespace OCLRT {
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

int hwMetricsEnableFuncFailing(InstrEscCbData cbData, int enable) {
    PerfCounterFlags::escHwMetricsCalled++;
    PerfCounterFlags::hwMetricsEnableStatus = enable;
    return -1;
}
int hwMetricsEnableFuncPassing(InstrEscCbData cbData, int enable) {
    PerfCounterFlags::escHwMetricsCalled++;
    PerfCounterFlags::hwMetricsEnableStatus = enable;
    return 0;
}
int autoSamplingStart(InstrEscCbData cbData, void **ppOAInterface) {
    PerfCounterFlags::autoSamplingFuncCalled++;
    PerfCounterFlags::autoSamplingStarted++;
    ppOAInterface[0] = new char[1];
    return 0;
}
int autoSamplingStartFailing(InstrEscCbData cbData, void **ppOAInterface) {
    PerfCounterFlags::autoSamplingFuncCalled++;
    PerfCounterFlags::autoSamplingStarted++;
    ppOAInterface[0] = nullptr;
    return -1;
}
int autoSamplingStop(void **ppOAInterface) {
    PerfCounterFlags::autoSamplingFuncCalled++;
    PerfCounterFlags::autoSamplingStopped++;
    if (ppOAInterface[0]) {
        delete[] static_cast<char *>(ppOAInterface[0]);
        ppOAInterface[0] = nullptr;
        return 0;
    }
    return -1;
}
int getPmRegsCfgPassing(InstrEscCbData cbData, uint32_t cfgId, InstrPmRegsCfg *pCfg, InstrAutoSamplingMode *pAutoSampling) {
    PerfCounterFlags::getPmRegsCfgCalled++;
    if (cfgId == 1) {
        pCfg->readRegs.regsCount = 2;
        pCfg->readRegs.reg[0].bitSize = 32;
        pCfg->readRegs.reg[1].bitSize = 64;
    }
    return 0;
}
int getPmRegsCfgFailing(InstrEscCbData cbData, uint32_t cfgId, InstrPmRegsCfg *pCfg, InstrAutoSamplingMode *pAutoSampling) {
    PerfCounterFlags::getPmRegsCfgCalled++;
    return -1;
}
int checkPmRegsCfgPassing(InstrPmRegsCfg *pQueryPmRegsCfg, uint32_t *pLastPmRegsCfgHandle, const void *pASInterface) {
    PerfCounterFlags::checkPmRegsCfgCalled++;
    return 0;
}
int checkPmRegsCfgFailing(InstrPmRegsCfg *pQueryPmRegsCfg, uint32_t *pLastPmRegsCfgHandle, const void *pASInterface) {
    PerfCounterFlags::checkPmRegsCfgCalled++;
    return -1;
}
int loadPmRegsCfgPassing(InstrEscCbData cbData, InstrPmRegsCfg *pCfg, int hardwareAccess) {
    PerfCounterFlags::loadPmRegsCfgCalled++;
    return 0;
}
int loadPmRegsCfgFailing(InstrEscCbData cbData, InstrPmRegsCfg *pCfg, int hardwareAccess) {
    PerfCounterFlags::loadPmRegsCfgCalled++;
    return -1;
}

int setPmRegsCfgFuncPassing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pValues) {
    PerfCounterFlags::setPmRegsCfgCalled++;
    return 0;
}
int setPmRegsCfgFuncFailing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pValues) {
    PerfCounterFlags::setPmRegsCfgCalled++;
    return -1;
}
int sendReadRegsCfgFuncPassing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pBitSizes) {
    PerfCounterFlags::sendReadRegsCfgCalled++;
    return 0;
}
int sendReadRegsCfgFuncFailing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pBitSizes) {
    PerfCounterFlags::sendReadRegsCfgCalled++;
    return -1;
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
} // namespace OCLRT
