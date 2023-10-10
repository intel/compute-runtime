/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/standby/sysman_standby.h"

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/api/standby/sysman_standby_imp.h"

namespace L0 {
namespace Sysman {

StandbyHandleContext::~StandbyHandleContext() = default;
void StandbyHandleContext::createHandle(bool onSubdevice, uint32_t subDeviceId) {
    std::unique_ptr<Standby> pStandby = std::make_unique<StandbyImp>(pOsSysman, onSubdevice, subDeviceId);
    if (pStandby->isStandbyEnabled == true) {
        handleList.push_back(std::move(pStandby));
    }
}

ze_result_t StandbyHandleContext::init(uint32_t subDeviceCount) {
    if (subDeviceCount > 0) {
        for (uint32_t subDeviceId = 0; subDeviceId < subDeviceCount; subDeviceId++) {
            createHandle(true, subDeviceId);
        }
    } else {
        createHandle(false, 0);
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t StandbyHandleContext::standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) {
    std::call_once(initStandbyOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phStandby) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phStandby[i] = handleList[i]->toStandbyHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
