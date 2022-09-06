/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/firmware/firmware_imp.h"

namespace L0 {
class OsFirmware;
FirmwareHandleContext::~FirmwareHandleContext() {
    releaseFwHandles();
}

void FirmwareHandleContext::releaseFwHandles() {
    for (Firmware *pFirmware : handleList) {
        delete pFirmware;
    }
    handleList.clear();
}
void FirmwareHandleContext::createHandle(const std::string &fwType) {
    Firmware *pFirmware = new FirmwareImp(pOsSysman, fwType);
    handleList.push_back(pFirmware);
}

void FirmwareHandleContext::init() {
    std::vector<std::string> supportedFwTypes = {};
    OsFirmware::getSupportedFwTypes(supportedFwTypes, pOsSysman);
    for (const std::string &fwType : supportedFwTypes) {
        createHandle(fwType);
    }
}

ze_result_t FirmwareHandleContext::firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware) {
    std::call_once(initFirmwareOnce, [this]() {
        this->init();
        this->firmwareInitDone = true;
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phFirmware) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phFirmware[i] = handleList[i]->toHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}
} // namespace L0
