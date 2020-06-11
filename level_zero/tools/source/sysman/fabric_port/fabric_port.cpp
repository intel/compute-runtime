/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/fabric_port/fabric_port.h"

#include "level_zero/tools/source/sysman/fabric_port/fabric_port_imp.h"

namespace L0 {

FabricPortHandleContext::~FabricPortHandleContext() {
    for (FabricPort *pFabricPort : handleList) {
        delete pFabricPort;
    }
    handleList.clear();
}

ze_result_t FabricPortHandleContext::init() {
    uint32_t numPorts = FabricPortImp::numPorts(pOsSysman);

    for (uint32_t portNum = 0; portNum < numPorts; portNum++) {
        FabricPort *pFabricPort = new FabricPortImp(pOsSysman, portNum);
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
