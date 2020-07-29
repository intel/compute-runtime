/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"

#include "level_zero/tools/source/sysman/ras/ras_imp.h"

namespace L0 {

RasHandleContext::~RasHandleContext() {
    for (Ras *pRas : handleList) {
        delete pRas;
    }
}
void RasHandleContext::createHandle(zes_ras_error_type_t type) {
    Ras *pRas = new RasImp(pOsSysman, type);
    if (pRas->isRasErrorSupported == true) {
        handleList.push_back(pRas);
    } else {
        delete pRas;
    }
}
void RasHandleContext::init() {
    createHandle(ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    createHandle(ZES_RAS_ERROR_TYPE_CORRECTABLE);
}
ze_result_t RasHandleContext::rasGet(uint32_t *pCount,
                                     zes_ras_handle_t *phRas) {
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

} // namespace L0
