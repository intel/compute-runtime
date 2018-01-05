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
#include "config.h"
#include "mock_performance_counters_linux.h"
#include "unit_tests/os_interface/linux/drm_mock.h"
#include "unit_tests/os_interface/linux/mock_os_time_linux.h"

namespace OCLRT {
MockPerformanceCountersLinux::MockPerformanceCountersLinux(OSTime *osTime)
    : PerformanceCounters(osTime), PerformanceCountersLinux(osTime), MockPerformanceCounters(osTime) {
    dlopenFunc = dlopenMockPassing;
    dlsymFunc = dlsymMockPassing;
    dlcloseFunc = dlcloseMock;
    setPlatformInfoFunc = setPlatformInfo;
    PerfCounterFlagsLinux::resetPerfCountersFlags();
}
std::unique_ptr<PerformanceCounters> MockPerformanceCounters::create(OSTime *osTime) {
    return std::unique_ptr<PerformanceCounters>(new MockPerformanceCountersLinux(osTime));
}

int PerfCounterFlagsLinux::dlopenFuncCalled;
int PerfCounterFlagsLinux::dlsymFuncCalled;
int PerfCounterFlagsLinux::dlcloseFuncCalled;
int PerfCounterFlagsLinux::perfmonLoadConfigCalled;
int PerfCounterFlagsLinux::setPlatformInfoFuncCalled;

void *dlopenMockPassing(const char *filename, int flag) throw() {
    PerfCounterFlagsLinux::dlopenFuncCalled++;
    return new char[1];
}
void *dlopenMockFailing(const char *filename, int flag) throw() {
    PerfCounterFlagsLinux::dlopenFuncCalled++;
    return nullptr;
}
void *dlsymMockPassing(void *handle, const char *symbol) throw() {
    PerfCounterFlagsLinux::dlsymFuncCalled++;
    return (void *)perfmonLoadConfigMock;
}
void *dlsymMockFailing(void *handle, const char *symbol) throw() {
    PerfCounterFlagsLinux::dlsymFuncCalled++;
    return nullptr;
}
int dlcloseMock(void *handle) throw() {
    PerfCounterFlagsLinux::dlcloseFuncCalled++;
    if (handle) {
        delete[] static_cast<char *>(handle);
    }
    return 0;
}
uint32_t setPlatformInfo(uint32_t productId, void *pSkuTable) {
    PerfCounterFlagsLinux::setPlatformInfoFuncCalled++;
    return 0;
}
int getTimeFuncPassing(clockid_t clkId, struct timespec *tp) throw() {
    tp->tv_sec = 0;
    tp->tv_nsec = 1;
    return 0;
}
int perfmonLoadConfigMock(int fd, drm_intel_context *ctx, uint32_t *oaCfgId, uint32_t *gpCfgId) {
    PerfCounterFlagsLinux::perfmonLoadConfigCalled++;
    return 0;
}

void PerfCounterFlagsLinux::resetPerfCountersFlags() {
    PerfCounterFlags::resetPerfCountersFlags();
    PerfCounterFlagsLinux::dlopenFuncCalled = 0;
    PerfCounterFlagsLinux::dlsymFuncCalled = 0;
    PerfCounterFlagsLinux::dlcloseFuncCalled = 0;
    PerfCounterFlagsLinux::perfmonLoadConfigCalled = 0;
    PerfCounterFlagsLinux::setPlatformInfoFuncCalled = 0;
}
void PerformanceCountersFixture::createPerfCounters() {
    performanceCountersBase = std::unique_ptr<MockPerformanceCounters>(new MockPerformanceCountersLinux(osTimeBase.get()));
}
void PerformanceCountersFixture::createOsTime() {
    osTimeBase = std::unique_ptr<MockOSTimeLinux>(new MockOSTimeLinux(osInterfaceBase.get()));
}
void PerformanceCountersFixture::fillOsInterface() {
    osInterfaceBase->get()->setDrm(new Drm2());
}
void PerformanceCountersFixture::releaseOsInterface() {
    delete static_cast<Drm2 *>(osInterfaceBase->get()->getDrm());
}
} // namespace OCLRT
