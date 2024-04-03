/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt_xml_offsets.h"

namespace L0 {
namespace Sysman {

ze_result_t PlatformMonitoringTech::getKeyOffsetMap(std::map<std::string, std::pair<uint32_t, uint32_t>> &keyOffsetMap) {
    int indexCount = 0;
    for (uint32_t index = 0; index < PmtSysman::PmtMaxInterfaces; index++) {
        auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guidToIndexList[index]);
        if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
            continue;
        } else {
            indexCount++;
            std::map<std::string, uint32_t> tempKeyOffsetMap = keyOffsetMapEntry->second;
            std::map<std::string, uint32_t>::iterator it;
            for (it = tempKeyOffsetMap.begin(); it != tempKeyOffsetMap.end(); it++) {
                keyOffsetMap.insert(std::make_pair(it->first, std::make_pair(it->second, index)));
            }
        }
    }
    if (indexCount == 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0