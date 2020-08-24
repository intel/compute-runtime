/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/engine/os_engine.h"

namespace L0 {
class WddmEngineImp : public OsEngine {
  public:
    ze_result_t getActivity(zes_engine_stats_t *pStats) override;
    ze_result_t getProperties(zes_engine_properties_t &properties) override;
};
ze_result_t WddmEngineImp::getActivity(zes_engine_stats_t *pStats) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmEngineImp::getProperties(zes_engine_properties_t &properties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t OsEngine::getNumEngineTypeAndInstances(std::multimap<zes_engine_group_t, uint32_t> &engineGroupInstance, OsSysman *pOsSysman) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsEngine *OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance) {
    WddmEngineImp *pWddmEngineImp = new WddmEngineImp();
    return static_cast<OsEngine *>(pWddmEngineImp);
}

} // namespace L0
