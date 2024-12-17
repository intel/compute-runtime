/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/vf_management/sysman_vf_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {
namespace Sysman {

ze_result_t VfImp::vfGetCapabilities(zes_vf_exp_capabilities_t *pCapability) {
    *pCapability = capability;
    return ZE_RESULT_SUCCESS;
}
ze_result_t VfImp::vfGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) {
    return pOsVf->vfOsGetMemoryUtilization(pCount, pMemUtil);
}
ze_result_t VfImp::vfGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) {
    return pOsVf->vfOsGetEngineUtilization(pCount, pEngineUtil);
}

void VfImp::init() {
    pOsVf->vfOsGetCapabilities(&capability);
}

VfImp::VfImp(OsSysman *pOsSysman, uint32_t vfId) {
    pOsVf = OsVf::create(pOsSysman, vfId);
    UNRECOVERABLE_IF(nullptr == pOsVf);
    init();
};

VfImp::~VfImp() = default;

} // namespace Sysman
} // namespace L0
