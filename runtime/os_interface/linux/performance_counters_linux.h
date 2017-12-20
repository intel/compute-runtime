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
#include "os_interface.h"
#include "runtime/os_interface/performance_counters.h"
#include <dlfcn.h>

typedef struct _drm_intel_context drm_intel_context;

namespace OCLRT {

class PerformanceCountersLinux : virtual public PerformanceCounters {
  public:
    PerformanceCountersLinux(OSTime *osTime);
    ~PerformanceCountersLinux() override;
    void initialize(const HardwareInfo *hwInfo) override;
    void enableImpl() override;

  protected:
    virtual bool getPerfmonConfig(InstrPmRegsCfg *pCfg);
    bool verifyPmRegsCfg(InstrPmRegsCfg *pCfg, uint32_t *pLastPmRegsCfgHandle, bool *pLastPmRegsCfgPending) override;

    typedef int (*perfmonLoadConfig_t)(int fd, drm_intel_context *ctx, uint32_t *oaCfgId, uint32_t *gpCfgId);
    typedef void *(*dlopenFunc_t)(const char *, int);
    typedef void *(*dlsymFunc_t)(void *, const char *);

    void *mdLibHandle;

    perfmonLoadConfig_t perfmonLoadConfigFunc;
    dlopenFunc_t dlopenFunc = dlopen;
    dlsymFunc_t dlsymFunc = dlsym;
    decltype(&dlclose) dlcloseFunc = dlclose;
    decltype(&instrSetPlatformInfo) setPlatformInfoFunc = instrSetPlatformInfo;
};
} // namespace OCLRT
