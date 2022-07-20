/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/fabric_port.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/fabric_port/fabric_port_imp.h"

namespace L0 {

FabricPortHandleContext::FabricPortHandleContext(OsSysman *pOsSysman) {
    pFabricDevice = new FabricDeviceImp(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pFabricDevice);
    handleList.clear();
}

FabricPortHandleContext::~FabricPortHandleContext() {
    UNRECOVERABLE_IF(nullptr == pFabricDevice);
    for (FabricPort *pFabricPort : handleList) {
        delete pFabricPort;
    }
    handleList.clear();
    delete pFabricDevice;
    pFabricDevice = nullptr;
}

ze_result_t FabricPortHandleContext::init() {
    UNRECOVERABLE_IF(nullptr == pFabricDevice);
    uint32_t numPorts = pFabricDevice->getNumPorts();

    for (uint32_t portNum = 0; portNum < numPorts; portNum++) {
        FabricPort *pFabricPort = new FabricPortImp(pFabricDevice, portNum);
        UNRECOVERABLE_IF(nullptr == pFabricPort);
        handleList.push_back(pFabricPort);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricPortHandleContext::fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) {
    std::call_once(initFabricPortOnce, [this]() {
        this->init();
    });
    uint32_t handleListSize = static_cast<uint32_t>(handleList.size());
    uint32_t numToCopy = std::min(*pCount, handleListSize);
    if (0 == *pCount || *pCount > handleListSize) {
        *pCount = handleListSize;
    }
    if (nullptr != phPort) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            phPort[i] = handleList[i]->toZesHandle();
        }
    }
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
