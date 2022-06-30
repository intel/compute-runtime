/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "standby_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include <cmath>

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
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
    pOsStandby = OsStandby::create(pOsSysman, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE, deviceProperties.subdeviceId);
    UNRECOVERABLE_IF(nullptr == pOsStandby);
    init();
}

StandbyImp::~StandbyImp() {
    delete pOsStandby;
}

} // namespace L0
