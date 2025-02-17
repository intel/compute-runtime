/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/engine/os_engine.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {
class KmdSysManager;
class WddmEngineImp : public OsEngine, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getActivity(zes_engine_stats_t *pStats) override;
    ze_result_t getActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) override;
    ze_result_t getProperties(zes_engine_properties_t &properties) override;
    ze_result_t isEngineModuleSupported() override;

    WddmEngineImp() = default;
    WddmEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId);
    ~WddmEngineImp() override = default;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    ze_bool_t isSingle = false;
    uint32_t engineInstance = 0u;
    zes_engine_group_t engineGroup = ZES_ENGINE_GROUP_ALL;
};
} // namespace L0
