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

ze_result_t InfoLogImp::infoLogEnable(zes_intel_info_log_enable_descriptor_exp *pEnableDescriptor) {
    return pOsInfoLog->infoLogEnable(pEnableDescriptor);
}

ze_result_t InfoLogImp::infoLogDisable() {
    return pOsInfoLog->infoLogDisable();
}

ze_result_t InfoLogImp::infoLogReadWithMetaData(uint32_t *pSize, uint8_t *pBuffer,
                                                uint32_t *pEventCount, zes_intel_info_log_metadata_exp *pDescriptors) {
    return pOsInfoLog->infoLogReadWithMetaData(pSize, pBuffer, pEventCount, pDescriptors);
}

void InfoLogImp::init() {
    pOsInfoLog->getProperties(&infoLogProperties);
}

InfoLogImp::InfoLogImp(zes_intel_info_log_format_exp_t format) {
    pOsInfoLog = OsInfoLog::create(format);
    init();
}

} // namespace Sysman
} // namespace L0
