/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "standby_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {

ze_result_t StandbyImp::standbyGetProperties(zes_standby_properties_t *pProperties) {
    *pProperties = standbyProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t StandbyImp::standbyGetMode(zes_standby_promo_mode_t *pMode) {
    return pOsStandby->getMode(*pMode);
}

ze_result_t StandbyImp::standbySetMode(const zes_standby_promo_mode_t mode) {
    return pOsStandby->setMode(mode);
}

void StandbyImp::init() {
    pOsStandby->osStandbyGetProperties(standbyProperties);
    this->isStandbyEnabled = pOsStandby->isStandbySupported();
}

StandbyImp::StandbyImp(OsSysman *pOsSysman, ze_device_handle_t handle) : deviceHandle(handle) {
    uint32_t subdeviceId = 0;
    ze_bool_t onSubdevice = false;
    SysmanDeviceImp::getSysmanDeviceInfo(deviceHandle, subdeviceId, onSubdevice, true);
    pOsStandby = OsStandby::create(pOsSysman, onSubdevice, subdeviceId);
    UNRECOVERABLE_IF(nullptr == pOsStandby);
    init();
}

StandbyImp::~StandbyImp() {}
} // namespace L0
