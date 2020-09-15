/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/engine/windows/os_engine_imp.h"

namespace L0 {

ze_result_t WddmEngineImp::getActivity(zes_engine_stats_t *pStats) {
    uint64_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::ActivityComponent;

    switch (this->engineGroup) {
    case ZES_ENGINE_GROUP_ALL:
        request.paramInfo = KmdSysman::ActivityDomainsType::ActitvityDomainGT;
        break;
    case ZES_ENGINE_GROUP_COMPUTE_ALL:
        request.paramInfo = KmdSysman::ActivityDomainsType::ActivityDomainRenderCompute;
        break;
    case ZES_ENGINE_GROUP_MEDIA_ALL:
        request.paramInfo = KmdSysman::ActivityDomainsType::ActivityDomainMedia;
        break;
    default:
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    request.requestId = KmdSysman::Requests::Activity::CurrentActivityCounter;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&value, sizeof(uint64_t), response.dataBuffer, sizeof(uint64_t));
    pStats->activeTime = value;
    memcpy_s(&value, sizeof(uint64_t), (response.dataBuffer + sizeof(uint64_t)), sizeof(uint64_t));
    pStats->timestamp = value;

    return status;
}

ze_result_t WddmEngineImp::getProperties(zes_engine_properties_t &properties) {
    properties.type = engineGroup;
    properties.onSubdevice = false;
    properties.subdeviceId = 0;
    return ZE_RESULT_SUCCESS;
}

WddmEngineImp::WddmEngineImp(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    this->engineGroup = engineType;
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsEngine *OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance) {
    WddmEngineImp *pWddmEngineImp = new WddmEngineImp(pOsSysman, engineType, engineInstance);
    return static_cast<OsEngine *>(pWddmEngineImp);
}

ze_result_t OsEngine::getNumEngineTypeAndInstances(std::multimap<zes_engine_group_t, uint32_t> &engineGroupInstance, OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();

    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::ActivityComponent;
    request.requestId = KmdSysman::Requests::Activity::NumActivityDomains;

    ze_result_t status = pKmdSysManager->requestSingle(request, response);

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    uint32_t maxNumEnginesSupported = 0;
    memcpy_s(&maxNumEnginesSupported, sizeof(uint32_t), response.dataBuffer, sizeof(uint32_t));

    if (maxNumEnginesSupported == 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    for (uint32_t i = 0; i < maxNumEnginesSupported; i++) {
        engineGroupInstance.insert({static_cast<zes_engine_group_t>(i), 0});
    }

    return status;
}

} // namespace L0
