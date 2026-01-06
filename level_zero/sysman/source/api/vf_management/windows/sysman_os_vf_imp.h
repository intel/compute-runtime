/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/api/vf_management/sysman_vf_imp.h"

namespace L0 {
namespace Sysman {

class WddmVfImp : public OsVf {
  public:
    ze_result_t vfOsGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) override;
    ze_result_t vfOsGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) override;
    ze_result_t vfOsGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) override;
    bool vfOsGetLocalMemoryUsed(uint64_t &lMemUsed) override;
    static uint32_t numEnabledVfs;
    static bool localMemoryUsedStatus;
    WddmVfImp() = default;
    ~WddmVfImp() = default;
};

} // namespace Sysman
} // namespace L0
