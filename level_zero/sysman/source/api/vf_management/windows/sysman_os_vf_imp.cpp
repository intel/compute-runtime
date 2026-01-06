/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/vf_management/windows/sysman_os_vf_imp.h"

#include "level_zero/sysman/source/api/vf_management/sysman_vf_imp.h"

namespace L0 {
namespace Sysman {

uint32_t WddmVfImp::numEnabledVfs = 0;

ze_result_t WddmVfImp::vfOsGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) {
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

std::unique_ptr<OsVf> OsVf::create(
    OsSysman *pOsSysman, uint32_t vfId) {
    std::unique_ptr<WddmVfImp> pWddmVfImp = std::make_unique<WddmVfImp>();
    return pWddmVfImp;
}

uint32_t OsVf::getNumEnabledVfs(OsSysman *pOsSysman) {
    return WddmVfImp::numEnabledVfs;
}

} // namespace Sysman
} // namespace L0
