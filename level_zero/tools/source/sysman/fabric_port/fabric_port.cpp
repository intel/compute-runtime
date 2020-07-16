/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/fabric_port.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/tools/source/sysman/fabric_port/fabric_port_imp.h"

namespace L0 {

FabricPortHandleContext::~FabricPortHandleContext() {
    if (nullptr != pFabricDevice) {
        for (FabricPort *pFabricPort : handleList) {
            delete pFabricPort;
        }
        handleList.clear();
        delete pFabricDevice;
        pFabricDevice = nullptr;
    }
}

ze_result_t FabricPortHandleContext::init() {
    pFabricDevice = new FabricDeviceImp(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pFabricDevice);

    uint32_t numPorts = pFabricDevice->getNumPorts();

    for (uint32_t portNum = 0; portNum < numPorts; portNum++) {
        FabricPort *pFabricPort = new FabricPortImp(pFabricDevice, portNum);
        UNRECOVERABLE_IF(nullptr == pFabricPort);
        handleList.push_back(pFabricPort);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t FabricPortHandleContext::fabricPortGet(uint32_t *pCount, zet_sysman_fabric_port_handle_t *phPort) {
    if (nullptr == phPort) {
        *pCount = static_cast<uint32_t>(handleList.size());
        return ZE_RESULT_SUCCESS;
    }
    uint32_t i = 0;
    for (FabricPort *pFabricPort : handleList) {
        if (i >= *pCount)
            break;
        phPort[i++] = pFabricPort->toHandle();
    }
    *pCount = i;
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
