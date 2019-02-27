/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/performance_counters.h"

#include "os_interface.h"

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
