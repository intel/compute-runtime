/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"

namespace L0 {
namespace Sysman {

ze_result_t PlatformMonitoringTech::getKeyOffsetMap(SysmanProductHelper *pSysmanProductHelper, std::string guid, std::map<std::string, uint64_t> &keyOffsetMap) {
    ze_result_t retVal = ZE_RESULT_ERROR_UNKNOWN;
    auto pGuidToKeyOffsetMap = pSysmanProductHelper->getGuidToKeyOffsetMap();
    if (pGuidToKeyOffsetMap == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto keyOffsetMapEntry = pGuidToKeyOffsetMap->find(guid);
    if (keyOffsetMapEntry == pGuidToKeyOffsetMap->end()) {
        // We didnt have any entry for this guid in guidToKeyOffsetMap
        retVal = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return retVal;
    }
    keyOffsetMap = keyOffsetMapEntry->second;
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0