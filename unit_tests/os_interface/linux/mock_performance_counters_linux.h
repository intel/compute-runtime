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

#pragma once
#include "runtime/os_interface/linux/performance_counters_linux.h"
#include "unit_tests/os_interface/mock_performance_counters.h"

namespace OCLRT {

void *dlopenMockPassing(const char *filename, int flag) throw();
void *dlopenMockFailing(const char *filename, int flag) throw();
void *dlsymMockPassing(void *handle, const char *symbol) throw();
void *dlsymMockFailing(void *handle, const char *symbol) throw();
int getTimeFuncPassing(clockid_t clk_id, struct timespec *tp) throw();
int dlcloseMock(void *handle) throw();
uint32_t setPlatformInfo(uint32_t productId, void *pSkuTable);
int perfmonLoadConfigMock(int fd, drm_intel_context *ctx, uint32_t *oa_cfg_id, uint32_t *gp_cfg_id);

class PerfCounterFlagsLinux : public PerfCounterFlags {
  public:
    static int dlopenFuncCalled;
    static int dlsymFuncCalled;
    static int dlcloseFuncCalled;
    static int perfmonLoadConfigCalled;
    static int setPlatformInfoFuncCalled;
    static void resetPerfCountersFlags();
};

class MockPerformanceCountersLinux : public PerformanceCountersLinux,
                                     public MockPerformanceCounters {
  public:
    MockPerformanceCountersLinux(OSTime *osTime);
    uint32_t getReportId() override {
        return MockPerformanceCounters::getReportId();
    }
    void initialize(const HardwareInfo *hwInfo) override {
        MockPerformanceCounters::initialize(hwInfo);
        return PerformanceCountersLinux::initialize(hwInfo);
    }
    void enableImpl() override {
        return PerformanceCountersLinux::enableImpl();
    }
    bool verifyPmRegsCfg(InstrPmRegsCfg *pCfg, uint32_t *pLastPmRegsCfgHandle, bool *pLastPmRegsCfgPending) override {
        return PerformanceCountersLinux::verifyPmRegsCfg(pCfg, pLastPmRegsCfgHandle, pLastPmRegsCfgPending);
    }
    bool getPerfmonConfig(InstrPmRegsCfg *pCfg) override {
        return PerformanceCountersLinux::getPerfmonConfig(pCfg);
    }
    void setDlopenFunc(dlopenFunc_t func) {
        dlopenFunc = func;
    }
    void setDlsymFunc(dlsymFunc_t func) {
        dlsymFunc = func;
    }
    void setPerfmonLoadConfigFunc(perfmonLoadConfig_t func) {
        perfmonLoadConfigFunc = func;
    }
};
} // namespace OCLRT
