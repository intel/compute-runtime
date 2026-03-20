/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_nl_api_stub.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <map>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {

std::map<zes_ras_error_category_exp_t, std::vector<std::string>> categoryToListOfErrors = {
    {ZES_RAS_ERROR_CATEGORY_EXP_COMPUTE_ERRORS, {"core-compute"}},
    {ZES_RAS_ERROR_CATEGORY_EXP_NON_COMPUTE_ERRORS, {"soc-internal"}},
};

std::vector<DrmRasNode> NetlinkRasUtil::rasNodes;
std::unique_ptr<DrmNlApi> (*NetlinkRasUtil::createDrmNlApi)() = nullptr;

void NetlinkRasUtil::getSupportedRasErrorTypes(std::set<zes_ras_error_type_t> &errorType, LinuxSysmanImp *pLinuxSysmanImp, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    errorType.insert(ZES_RAS_ERROR_TYPE_CORRECTABLE);
    errorType.insert(ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
}

uint32_t NetlinkRasUtil::rasGetCategoryCount() {
    return static_cast<uint32_t>(categoryToListOfErrors.size());
}

ze_result_t NetlinkRasUtil::rasGetState(zes_ras_state_t &state, ze_bool_t clear) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t NetlinkRasUtil::rasGetStateExp(uint32_t numCategoriesRequested, zes_ras_state_exp_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t NetlinkRasUtil::rasClearStateExp(zes_ras_error_category_exp_t category) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

NetlinkRasUtil::NetlinkRasUtil(zes_ras_error_type_t type, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) : pLinuxSysmanImp(pLinuxSysmanImp), rasErrorType(type) {
}

NetlinkRasUtil::~NetlinkRasUtil() {
}

} // namespace Sysman
} // namespace L0
