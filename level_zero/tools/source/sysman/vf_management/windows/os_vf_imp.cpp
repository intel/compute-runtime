/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/vf_management/windows/os_vf_imp.h"

#include "level_zero/tools/source/sysman/vf_management/vf_imp.h"

namespace L0 {

uint32_t WddmVfImp::numEnabledVfs = 0;

ze_result_t WddmVfImp::vfOsGetCapabilities(zes_vf_exp_capabilities_t *pCapability) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
ze_result_t WddmVfImp::vfOsGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
ze_result_t WddmVfImp::vfOsGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
bool WddmVfImp::vfOsGetLocalMemoryUsed(uint64_t &lMmemUsed) {
    return false;
}
bool WddmVfImp::vfOsGetLocalMemoryQuota(uint64_t &lMemQuota) {
    return false;
}

std::unique_ptr<OsVf> OsVf::create(
    OsSysman *pOsSysman, uint32_t vfId) {
    std::unique_ptr<WddmVfImp> pWddmVfImp = std::make_unique<WddmVfImp>();
    return pWddmVfImp;
}

uint32_t OsVf::getNumEnabledVfs(OsSysman *pOsSysman) {
    return WddmVfImp::numEnabledVfs;
}

} // namespace L0
