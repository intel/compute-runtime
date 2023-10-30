/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "level_zero/sysman/source/api/ras/sysman_ras_imp.h"
#include "level_zero/sysman/source/device/os_sysman.h"

namespace L0 {
namespace Sysman {

void RasHandleContext::releaseRasHandles() {
    for (Ras *pRas : handleList) {
        delete pRas;
    }
    handleList.clear();
}

RasHandleContext::~RasHandleContext() {
    releaseRasHandles();
}

void RasHandleContext::createHandle(zes_ras_error_type_t type, ze_bool_t isSubDevice, uint32_t subDeviceId) {
    Ras *pRas = new RasImp(pOsSysman, type, isSubDevice, subDeviceId);
    handleList.push_back(pRas);
}

void RasHandleContext::init(uint32_t subDeviceCount) {
    const auto isSubDevice = (subDeviceCount > 0);
    uint32_t subDeviceCountLimit = (isSubDevice) ? subDeviceCount - 1 : 0;
    for (uint32_t subDeviceId = 0; subDeviceId <= subDeviceCountLimit; subDeviceId++) {
        std::set<zes_ras_error_type_t> errorTypeSubDev;
        OsRas::getSupportedRasErrorTypes(errorTypeSubDev, pOsSysman, isSubDevice, subDeviceId);
        if (errorTypeSubDev.size() == 0) {
            return;
        }

        for (const auto &type : errorTypeSubDev) {
            createHandle(type, isSubDevice, subDeviceId);
        }
    }
}

ze_result_t RasHandleContext::rasGet(uint32_t *pCount,
                                     zes_ras_handle_t *phRas) {
    std::call_once(initRasOnce, [this]() {
        this->init(pOsSysman->getSubDeviceCount());
        this->rasInitDone = true;
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phRas) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phRas[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace Sysman
} // namespace L0
