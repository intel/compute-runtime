/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/memory/windows/os_memory_imp.h"

namespace L0 {
bool WddmMemoryImp::isMemoryModuleSupported() {
    return pDevice->getDriverHandle()->getMemoryManager()->isLocalMemorySupported(pDevice->getRootDeviceIndex());
}
ze_result_t WddmMemoryImp::getProperties(zes_mem_properties_t *pProperties) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t valueSmall = 0;
    uint64_t valueLarge = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subdeviceId;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MemoryComponent;
    request.requestId = KmdSysman::Requests::Memory::MemoryType;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    switch (valueSmall) {
    case KmdSysman::MemoryType::DDR4: {
        pProperties->type = ZES_MEM_TYPE_DDR4;
    } break;
    case KmdSysman::MemoryType::DDR5: {
        pProperties->type = ZES_MEM_TYPE_DDR5;
    } break;
    case KmdSysman::MemoryType::LPDDR5: {
        pProperties->type = ZES_MEM_TYPE_LPDDR5;
    } break;
    case KmdSysman::MemoryType::LPDDR4: {
        pProperties->type = ZES_MEM_TYPE_LPDDR4;
    } break;
    case KmdSysman::MemoryType::DDR3: {
        pProperties->type = ZES_MEM_TYPE_DDR3;
    } break;
    case KmdSysman::MemoryType::LPDDR3: {
        pProperties->type = ZES_MEM_TYPE_LPDDR3;
    } break;
    default: {
        pProperties->type = ZES_MEM_TYPE_FORCE_UINT32;
    } break;
    }

    request.requestId = KmdSysman::Requests::Memory::PhysicalSize;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueLarge, sizeof(uint64_t), response.dataBuffer, sizeof(uint64_t));
    pProperties->physicalSize = valueLarge;

    request.requestId = KmdSysman::Requests::Memory::NumChannels;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    pProperties->numChannels = valueSmall;

    request.requestId = KmdSysman::Requests::Memory::MemoryLocation;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    pProperties->location = static_cast<zes_mem_loc_t>(valueSmall);

    request.requestId = KmdSysman::Requests::Memory::MemoryWidth;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    pProperties->busWidth = valueSmall;

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmMemoryImp::getBandwidth(zes_mem_bandwidth_t *pBandwidth) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t valueSmall = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    pBandwidth->writeCounter = 0;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MemoryComponent;
    request.requestId = KmdSysman::Requests::Memory::MaxBandwidth;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    pBandwidth->maxBandwidth = valueSmall;

    request.requestId = KmdSysman::Requests::Memory::CurrentBandwidthRead;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueSmall, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));
    pBandwidth->readCounter = valueSmall * MbpsToBytesPerSecond;

    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    pBandwidth->timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();

    return ZE_RESULT_SUCCESS;
}

ze_result_t WddmMemoryImp::getState(zes_mem_state_t *pState) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint64_t valueLarge = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    pState->health = ZES_MEM_HEALTH_OK;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::MemoryComponent;
    request.requestId = KmdSysman::Requests::Memory::PhysicalSize;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueLarge, sizeof(uint64_t), response.dataBuffer, sizeof(uint64_t));
    pState->size = valueLarge;

    request.requestId = KmdSysman::Requests::Memory::CurrentFreeMemorySize;

    status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&valueLarge, sizeof(uint64_t), response.dataBuffer, sizeof(uint64_t));
    pState->free = valueLarge;

    return ZE_RESULT_SUCCESS;
}

WddmMemoryImp::WddmMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
    pDevice = pWddmSysmanImp->getDeviceHandle();
}

OsMemory *OsMemory::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    WddmMemoryImp *pWddmMemoryImp = new WddmMemoryImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsMemory *>(pWddmMemoryImp);
}

} // namespace L0
