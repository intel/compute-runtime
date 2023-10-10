/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/fan/sysman_fan_imp.h"

#include "shared/source/helpers/debug_helpers.h"

namespace L0 {
namespace Sysman {

FanImp::FanImp(OsSysman *pOsSysman) {
    pOsFan = OsFan::create(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pOsFan);

    init();
}

ze_result_t FanImp::fanGetProperties(zes_fan_properties_t *pProperties) {
    return pOsFan->getProperties(pProperties);
}

ze_result_t FanImp::fanGetConfig(zes_fan_config_t *pConfig) {
    return pOsFan->getConfig(pConfig);
}

ze_result_t FanImp::fanSetDefaultMode() {
    return pOsFan->setDefaultMode();
}

ze_result_t FanImp::fanSetFixedSpeedMode(const zes_fan_speed_t *pSpeed) {
    return pOsFan->setFixedSpeedMode(pSpeed);
}

ze_result_t FanImp::fanSetSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) {
    return pOsFan->setSpeedTableMode(pSpeedTable);
}

ze_result_t FanImp::fanGetState(zes_fan_speed_units_t units, int32_t *pSpeed) {
    return pOsFan->getState(units, pSpeed);
}

void FanImp::init() {
    if (pOsFan->isFanModuleSupported()) {
        this->initSuccess = true;
    }
}

FanImp::~FanImp() = default;

} // namespace Sysman
} // namespace L0
