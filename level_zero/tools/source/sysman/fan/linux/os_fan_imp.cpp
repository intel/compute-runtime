/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fan/linux/os_fan_imp.h"

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

ze_result_t LinuxFanImp::getProperties(zes_fan_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
ze_result_t LinuxFanImp::getConfig(zes_fan_config_t *pConfig) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFanImp::setDefaultMode() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFanImp::setFixedSpeedMode(const zes_fan_speed_t *pSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFanImp::setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFanImp::getState(zes_fan_speed_units_t units, int32_t *pSpeed) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool LinuxFanImp::isFanModuleSupported() {
    return false;
}

LinuxFanImp::LinuxFanImp(OsSysman *pOsSysman) {
}

OsFan *OsFan::create(OsSysman *pOsSysman) {
    LinuxFanImp *pLinuxFanImp = new LinuxFanImp(pOsSysman);
    return static_cast<OsFan *>(pLinuxFanImp);
}

} // namespace L0
