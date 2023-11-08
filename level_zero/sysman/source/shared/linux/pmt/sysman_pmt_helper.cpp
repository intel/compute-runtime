/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt_xml_offsets.h"

namespace L0 {
namespace Sysman {

ze_result_t PlatformMonitoringTech::getKeyOffsetMap(std::string guid, std::map<std::string, uint64_t> &keyOffsetMap) {
    ze_result_t retVal = ZE_RESULT_ERROR_UNKNOWN;
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guid);
    if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
        // We didnt have any entry for this guid in guidToKeyOffsetMap
        retVal = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return retVal;
    }
    keyOffsetMap = keyOffsetMapEntry->second;
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0