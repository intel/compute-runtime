/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"

#include "shared/source/os_interface/linux/engine_info_impl.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

static const std::multimap<drm_i915_gem_engine_class, zes_engine_group_t> i915ToEngineMap = {
    {I915_ENGINE_CLASS_RENDER, ZES_ENGINE_GROUP_RENDER_SINGLE},
    {I915_ENGINE_CLASS_VIDEO, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE},
    {I915_ENGINE_CLASS_VIDEO, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE},
    {I915_ENGINE_CLASS_COPY, ZES_ENGINE_GROUP_COPY_SINGLE}};

ze_result_t OsEngine::getNumEngineTypeAndInstances(std::multimap<zes_engine_group_t, uint32_t> &engineGroupInstance, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    NEO::Drm *pDrm = &pLinuxSysmanImp->getDrm();

    if (pDrm->queryEngineInfo() == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto engineInfo = static_cast<NEO::EngineInfoImpl *>(pDrm->getEngineInfo());
    for (auto itr = engineInfo->engines.begin(); itr != engineInfo->engines.end(); ++itr) {
        auto L0EngineEntryInMap = i915ToEngineMap.find(static_cast<drm_i915_gem_engine_class>(itr->engine.engine_class));
        if (L0EngineEntryInMap == i915ToEngineMap.end()) {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        auto L0EngineType = L0EngineEntryInMap->second;
        engineGroupInstance.insert({L0EngineType, static_cast<uint32_t>(itr->engine.engine_instance)});
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImp::getActivity(zes_engine_stats_t *pStats) {
    pStats->timestamp = 0;
    pStats->activeTime = 0;
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxEngineImp::getProperties(zes_engine_properties_t &properties) {
    properties.type = engineGroup;
    properties.onSubdevice = false;
    properties.subdeviceId = 0;
    return ZE_RESULT_SUCCESS;
}

LinuxEngineImp::LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
    engineGroup = type;
    this->engineInstance = engineInstance;
}

OsEngine *OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance) {
    LinuxEngineImp *pLinuxEngineImp = new LinuxEngineImp(pOsSysman, type, engineInstance);
    return static_cast<OsEngine *>(pLinuxEngineImp);
}

} // namespace L0
