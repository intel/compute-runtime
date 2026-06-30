/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/info_log/sysman_info_log_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t InfoLogImp::infoLogGetProperties(zes_intel_info_log_properties_exp_t *pProperties) {
    *pProperties = infoLogProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t InfoLogImp::infoLogRead(uint32_t *pSize, uint8_t *pBuffer) {
    return pOsInfoLog->infoLogRead(pSize, pBuffer);
}

ze_result_t InfoLogImp::infoLogEnable(bool state) {
    return pOsInfoLog->infoLogEnable(state);
}

void InfoLogImp::init() {
    pOsInfoLog->getProperties(&infoLogProperties);
}

InfoLogImp::InfoLogImp() {
    pOsInfoLog = OsInfoLog::create();
    init();
}

} // namespace Sysman
} // namespace L0
