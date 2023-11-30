/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/fabric_port/sysman_fabric_port.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/api/fabric_port/sysman_fabric_port_imp.h"

namespace L0 {
namespace Sysman {

FabricPortHandleContext::FabricPortHandleContext(OsSysman *pOsSysman) {
    pFabricDevice = new FabricDeviceImp(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pFabricDevice);
    handleList.clear();
}

FabricPortHandleContext::~FabricPortHandleContext() {
    UNRECOVERABLE_IF(nullptr == pFabricDevice);
    handleList.clear();
    delete pFabricDevice;
    pFabricDevice = nullptr;
}

ze_result_t FabricPortHandleContext::init() {
    UNRECOVERABLE_IF(nullptr == pFabricDevice);
    uint32_t numPorts = pFabricDevice->getNumPorts();

    for (uint32_t portNum = 0; portNum < numPorts; portNum++) {
        std::unique_ptr<FabricPort> pFabricPort = std::make_unique<FabricPortImp>(pFabricDevice, portNum);
        UNRECOVERABLE_IF(nullptr == pFabricPort);
        handleList.push_back(std::move(pFabricPort));
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

ze_result_t FabricPortHandleContext::fabricPortGetMultiPortThroughput(uint32_t numPorts, zes_fabric_port_handle_t *phPort, zes_fabric_port_throughput_t **pThroughput) {
    if (!numPorts) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Invalid number of ports \n", __FUNCTION__);
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    std::vector<zes_fabric_port_id_t> portIds = {};
    zes_fabric_port_properties_t pProperties = {};
    for (uint32_t i = 0; i < numPorts; i++) {
        L0::Sysman::FabricPort::fromHandle(phPort[i])->fabricPortGetProperties(&pProperties);
        portIds.push_back(pProperties.portId);
    }
    return pFabricDevice->getOsFabricDevice()->getMultiPortThroughput(portIds, pThroughput);
}

} // namespace Sysman
} // namespace L0
