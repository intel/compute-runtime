/*
* Copyright (c) 2017 - 2018, Intel Corporation
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

#pragma once
#include "runtime/device/device.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/libult/create_command_stream.h"

namespace OCLRT {
class OSInterface;

bool hwMetricsEnableFuncPassing(InstrEscCbData cbData, bool enable);
bool hwMetricsEnableFuncFailing(InstrEscCbData cbData, bool enable);
bool autoSamplingStart(InstrEscCbData cbData, void **ppOAInterface);
bool autoSamplingStartFailing(InstrEscCbData cbData, void **ppOAInterface);
bool autoSamplingStop(void **ppOAInterface);
bool getPmRegsCfgPassing(InstrEscCbData cbData, uint32_t cfgId, InstrPmRegsCfg *pCfg, InstrAutoSamplingMode *pAutoSampling);
bool getPmRegsCfgFailing(InstrEscCbData cbData, uint32_t cfgId, InstrPmRegsCfg *pCfg, InstrAutoSamplingMode *pAutoSampling);
bool checkPmRegsCfgPassing(InstrPmRegsCfg *pQueryPmRegsCfg, uint32_t *pLastPmRegsCfgHandle, const void *pASInterface);
bool checkPmRegsCfgFailing(InstrPmRegsCfg *pQueryPmRegsCfg, uint32_t *pLastPmRegsCfgHandle, const void *pASInterface);
bool loadPmRegsCfgPassing(InstrEscCbData cbData, InstrPmRegsCfg *pCfg, bool hardwareAccess);
bool loadPmRegsCfgFailing(InstrEscCbData cbData, InstrPmRegsCfg *pCfg, bool hardwareAccess);
template <typename GTDI_QUERY, typename HwPerfCountersLayout>
void getPerfCountersQueryData(InstrEscCbData cbData, GTDI_QUERY *pData, HwPerfCountersLayout *pLayout, uint64_t cpuRawTimestamp, void *pASInterface, InstrPmRegsCfg *pPmRegsCfg, bool useMiRPC, bool resetASData = false, const InstrAllowedContexts *pAllowedContexts = NULL);
bool setPmRegsCfgFuncPassing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pValues);
bool setPmRegsCfgFuncFailing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pValues);
bool sendReadRegsCfgFuncPassing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pBitSizes);
bool sendReadRegsCfgFuncFailing(InstrEscCbData cbData, uint32_t count, uint32_t *pOffsets, uint32_t *pBitSizes);

class PerfCounterFlags {
  public:
    static int autoSamplingStarted;
    static int autoSamplingStopped;
    static int autoSamplingFuncCalled;
    static int checkPmRegsCfgCalled;
    static int escHwMetricsCalled;
    static int getPerfCountersQueryDataCalled;
    static int getPmRegsCfgCalled;
    static int hwMetricsEnableStatus;
    static int initalizeCalled;
    static int loadPmRegsCfgCalled;
    static int setPmRegsCfgCalled;
    static int sendReadRegsCfgCalled;
    static void resetPerfCountersFlags();
};

class MockPerformanceCounters : virtual public PerformanceCounters {
  public:
    MockPerformanceCounters(OSTime *osTime);
    static std::unique_ptr<PerformanceCounters> create(OSTime *osTime);

    void initialize(const HardwareInfo *hwInfo) override;

    void setEscHwMetricsFunc(decltype(hwMetricsEnableFunc) func) {
        hwMetricsEnableFunc = func;
    }
    void setAutoSamplingStartFunc(decltype(autoSamplingStartFunc) func) {
        autoSamplingStartFunc = func;
    }
    void setGetPmRegsCfgFunc(decltype(getPmRegsCfgFunc) func) {
        getPmRegsCfgFunc = func;
    }
    void setCheckPmRegsCfgFunc(decltype(checkPmRegsCfgFunc) func) {
        checkPmRegsCfgFunc = func;
    }
    void setLoadPmRegsCfgFunc(decltype(loadPmRegsCfgFunc) func) {
        loadPmRegsCfgFunc = func;
    }
    void setSetPmRegsCfgFunc(decltype(setPmRegsCfgFunc) func) {
        setPmRegsCfgFunc = func;
    }
    void setSendReadRegsCfgFunc(decltype(sendReadRegsCfgFunc) func) {
        sendReadRegsCfgFunc = func;
    }
    uint64_t getCpuRawTimestamp() const {
        return cpuRawTimestamp;
    }
    void setAsIface(void *asIface) {
        pAutoSamplingInterface = asIface;
    }
    void setGfxFamily(GFXCORE_FAMILY family) {
        gfxFamily = family;
    }
    uint32_t getReportId() override {
        return PerformanceCounters::getReportId();
    }
    void setAvailableFlag(bool value) {
        available = value;
    }
    GFXCORE_FAMILY getGfxFamily() {
        return gfxFamily;
    }
    InstrEscCbData getCbData() {
        return cbData;
    }
    bool getHwMetricsEnabled() {
        return hwMetricsEnabled;
    }
    bool getUseMIRPC() {
        return useMIRPC;
    }
    void *getPAutoSamplingInterface() {
        return pAutoSamplingInterface;
    }
    uint64_t getCpuRawTimestamp() {
        return cpuRawTimestamp;
    }
    bool getAvailable() {
        return available;
    }
};

struct PerformanceCountersDeviceFixture {
    virtual void SetUp() {
        overrideCSR = overrideCommandStreamReceiverCreation;
        createFunc = Device::createPerformanceCountersFunc;
        overrideCommandStreamReceiverCreation = true;
        Device::createPerformanceCountersFunc = MockPerformanceCounters::create;
    }
    virtual void TearDown() {
        overrideCommandStreamReceiverCreation = overrideCSR;
        Device::createPerformanceCountersFunc = createFunc;
    }
    bool overrideCSR;
    decltype(&PerformanceCounters::create) createFunc;
};

struct PerformanceCountersFixture {

    virtual void SetUp();

    virtual void TearDown() {
        releaseOsInterface();
    }

    virtual void createPerfCounters();
    void createOsTime();
    void fillOsInterface();
    void releaseOsInterface();

    std::unique_ptr<OSInterface> osInterfaceBase;
    std::unique_ptr<OSTime> osTimeBase;
    std::unique_ptr<MockPerformanceCounters> performanceCountersBase;
};
} // namespace OCLRT
