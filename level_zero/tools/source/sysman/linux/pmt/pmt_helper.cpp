/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"
#include "level_zero/tools/source/sysman/linux/pmt/pmt_xml_offsets.h"

namespace L0 {

ze_result_t PlatformMonitoringTech::getKeyOffsetMap(std::string guid, std::map<std::string, uint64_t> &keyOffsetMap) {
    ze_result_t retVal = ZE_RESULT_ERROR_UNKNOWN;
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guid);
    if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
        // We did not have any entry for this guid in guidToKeyOffsetMap
        retVal = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return retVal;
    }
    keyOffsetMap = keyOffsetMapEntry->second;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0