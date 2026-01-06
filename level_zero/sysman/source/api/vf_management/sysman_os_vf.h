/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <memory>

namespace L0 {
namespace Sysman {
struct OsSysman;

class OsVf {
  public:
    virtual ze_result_t vfOsGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) = 0;
    virtual ze_result_t vfOsGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) = 0;
    virtual ze_result_t vfOsGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) = 0;
    virtual bool vfOsGetLocalMemoryUsed(uint64_t &lMemUsed) = 0;
    static std::unique_ptr<OsVf> create(OsSysman *pOsSysman, uint32_t vfId);
    static uint32_t getNumEnabledVfs(OsSysman *pOsSysman);
    virtual ~OsVf() = default;
};

} // namespace Sysman
} // namespace L0
