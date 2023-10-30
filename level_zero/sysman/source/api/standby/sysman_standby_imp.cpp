/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman_standby_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/device/sysman_device_imp.h"

namespace L0 {
namespace Sysman {

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

StandbyImp::StandbyImp(OsSysman *pOsSysman, bool onSubdevice, uint32_t subDeviceId) {
    pOsStandby = OsStandby::create(pOsSysman, onSubdevice, subDeviceId);
    init();
}

StandbyImp::~StandbyImp() {}
} // namespace Sysman
} // namespace L0
