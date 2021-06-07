/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"

namespace L0 {

const std::map<std::string, uint64_t> deviceKeyOffsetMap = {
    {"PACKAGE_ENERGY", 0x400},
    {"COMPUTE_TEMPERATURES", 0x68},
    {"SOC_TEMPERATURES", 0x60},
    {"CORE_TEMPERATURES", 0x6c}};

ze_result_t PlatformMonitoringTech::getKeyOffsetMap(std::string guid, std::map<std::string, uint64_t> &keyOffsetMap) {
    if (guid.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = deviceKeyOffsetMap;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0