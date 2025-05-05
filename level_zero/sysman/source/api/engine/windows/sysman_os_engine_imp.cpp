/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/engine/windows/sysman_os_engine_imp.h"

#include "level_zero/sysman/source/shared/windows/sysman_kmd_sys.h"
#include "level_zero/sysman/source/shared/windows/sysman_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {

static const std::map<zes_engine_group_t, KmdSysman::ActivityDomainsType> engineGroupToDomainTypeMap = {
    {ZES_ENGINE_GROUP_ALL, KmdSysman::ActivityDomainsType::ActitvityDomainGT},
    {ZES_ENGINE_GROUP_COMPUTE_ALL, KmdSysman::ActivityDomainsType::ActivityDomainRenderCompute},
    {ZES_ENGINE_GROUP_MEDIA_ALL, KmdSysman::ActivityDomainsType::ActivityDomainMedia},
    {ZES_ENGINE_GROUP_COPY_ALL, KmdSysman::ActivityDomainsType::ActivityDomainCopy},
    {ZES_ENGINE_GROUP_COMPUTE_SINGLE, KmdSysman::ActivityDomainsType::ActivityDomainComputeSingle},
    {ZES_ENGINE_GROUP_RENDER_SINGLE, KmdSysman::ActivityDomainsType::ActivityDomainRenderSingle},
    {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, KmdSysman::ActivityDomainsType::ActivityDomainMediaCodecSingle},
    {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, KmdSysman::ActivityDomainsType::ActivityDomainMediaCodecSingle},
    {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, KmdSysman::ActivityDomainsType::ActivityDomainMediaEnhancementSingle},
    {ZES_ENGINE_GROUP_COPY_SINGLE, KmdSysman::ActivityDomainsType::ActivityDomainCopySingle},
};

ze_result_t WddmEngineImp::getActivity(zes_engine_stats_t *pStats) {
    uint64_t activeTime = 0;
    uint64_t timeStamp = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;

    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::ActivityComponent;

    const auto &iterator = engineGroupToDomainTypeMap.find(this->engineGroup);
    if (iterator == engineGroupToDomainTypeMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    request.paramInfo = engineGroupToDomainTypeMap.at(this->engineGroup);
    request.requestId = KmdSysman::Requests::Activity::CurrentCounterV2;
    if (isSingle) {
        request.dataSize = sizeof(engineInstance);
        memcpy_s(request.dataBuffer, KmdSysman::MaxPropertyBufferSize, &engineInstance, sizeof(engineInstance));
    }

    ze_result_t status = pKmdSysManager->requestSingle(request, response);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    memcpy_s(&activeTime, sizeof(uint64_t), response.dataBuffer, sizeof(uint64_t));
    memcpy_s(&timeStamp, sizeof(uint64_t), (response.dataBuffer + sizeof(uint64_t)), sizeof(uint64_t));

    pStats->activeTime = activeTime;
    pStats->timestamp = timeStamp;

    return status;
}

ze_result_t WddmEngineImp::getActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmEngineImp::getProperties(zes_engine_properties_t &properties) {
    properties.type = engineGroup;
    properties.onSubdevice = false;
    properties.subdeviceId = 0;
    return ZE_RESULT_SUCCESS;
}

bool WddmEngineImp::isEngineModuleSupported() {
    return true;
}

WddmEngineImp::WddmEngineImp(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t tileId) : engineGroup(engineType), engineInstance(engineInstance) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);

    const std::vector singleEngineGroups = {
        ZES_ENGINE_GROUP_COMPUTE_SINGLE,
        ZES_ENGINE_GROUP_RENDER_SINGLE,
        ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE,
        ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE,
        ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE,
        ZES_ENGINE_GROUP_COPY_SINGLE};

    if (std::find(singleEngineGroups.begin(), singleEngineGroups.end(), engineType) != singleEngineGroups.end()) {
        isSingle = true;
    }

    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

std::unique_ptr<OsEngine> OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t engineType, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubDevice) {
    std::unique_ptr<WddmEngineImp> pWddmEngineImp = std::make_unique<WddmEngineImp>(pOsSysman, engineType, engineInstance, tileId);
    return pWddmEngineImp;
}

ze_result_t OsEngine::getNumEngineTypeAndInstances(std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> &engineGroupInstance, OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    KmdSysManager *pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();

    engineGroupInstance.clear();

    // create multiple requests for all the possible engine groups
    std::vector<KmdSysman::RequestProperty> vRequests{};
    std::vector<KmdSysman::ResponseProperty> vResponses{};

    KmdSysman::RequestProperty request = {};
    request.commandId = KmdSysman::Command::Get;
    request.componentId = KmdSysman::Component::ActivityComponent;
    request.requestId = KmdSysman::Requests::Activity::EngineInstancesPerGroup;

    for (auto &engineGroup : engineGroupToDomainTypeMap) {
        request.paramInfo = static_cast<uint32_t>(engineGroup.second);
        vRequests.push_back(request);
    }

    ze_result_t status = pKmdSysManager->requestMultiple(vRequests, vResponses);
    if ((status != ZE_RESULT_SUCCESS)) {
        return status;
    }

    if (vResponses.size() != vRequests.size()) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    uint32_t index = 0;
    for (auto &engineGroup : engineGroupToDomainTypeMap) {
        if (vResponses[index].dataSize > 0) {
            uint32_t instanceCount = 0;
            memcpy_s(&instanceCount, sizeof(uint32_t), vResponses[index].dataBuffer, sizeof(uint32_t));
            for (uint32_t instance = 0; instance < instanceCount; instance++) {
                engineGroupInstance.insert({engineGroup.first, {instance, 0}});
            }
        }
        index++;
    }

    return status;
}

void OsEngine::initGroupEngineHandleGroupFd(OsSysman *pOsSysman) {
    return;
}

void OsEngine::closeFdsForGroupEngineHandles(OsSysman *pOsSysman) {
    return;
}

} // namespace Sysman
} // namespace L0
