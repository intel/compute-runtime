/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/vf_management/vf_imp.h"

namespace L0 {

class WddmVfImp : public OsVf {
  public:
    ze_result_t vfOsGetCapabilities(zes_vf_exp_capabilities_t *pCapability) override;
    ze_result_t vfOsGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) override;
    ze_result_t vfOsGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) override;
    bool vfOsGetLocalMemoryQuota(uint64_t &lMemQuota) override;
    bool vfOsGetLocalMemoryUsed(uint64_t &lMemUsed) override;
    static uint32_t numEnabledVfs;
    static bool localMemoryUsedStatus;
    WddmVfImp() = default;
    ~WddmVfImp() = default;
};

} // namespace L0
