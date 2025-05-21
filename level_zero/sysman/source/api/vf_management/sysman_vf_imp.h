/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/vf_management/sysman_os_vf.h"
#include "level_zero/sysman/source/api/vf_management/sysman_vf_management.h"
#include <level_zero/zes_api.h>

#include <memory>

namespace L0 {
namespace Sysman {

class VfImp : public VfManagement, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t vfGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) override;
    ze_result_t vfGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) override;
    ze_result_t vfGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) override;

    VfImp() = default;
    VfImp(OsSysman *pOsSysman, uint32_t vfId);
    ~VfImp() override;

  protected:
    std::unique_ptr<OsVf> pOsVf;
};

} // namespace Sysman
} // namespace L0
