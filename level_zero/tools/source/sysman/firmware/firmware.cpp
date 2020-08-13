/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "firmware.h"

#include "shared/source/helpers/basic_math.h"

#include "firmware_imp.h"

namespace L0 {

FirmwareHandleContext::~FirmwareHandleContext() {
    for (Firmware *pFirmware : handleList) {
        delete pFirmware;
    }
    handleList.clear();
}

void FirmwareHandleContext::init() {
    Firmware *pFirmware = new FirmwareImp(pOsSysman);
    if (pFirmware->isFirmwareEnabled == true) {
        handleList.push_back(pFirmware);
    } else {
        delete pFirmware;
    }
}

ze_result_t FirmwareHandleContext::firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware) {
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
