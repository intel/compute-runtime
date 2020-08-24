/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "sysman/engine/os_engine.h"
namespace L0 {
class LinuxEngineImp : public OsEngine, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getActivity(zes_engine_stats_t *pStats) override;
    ze_result_t getProperties(zes_engine_properties_t &properties) override;
    LinuxEngineImp() = default;
    LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance);
    ~LinuxEngineImp() override = default;

  protected:
    zes_engine_group_t engineGroup = ZES_ENGINE_GROUP_ALL;
    uint32_t engineInstance = 0;
    NEO::Drm *pDrm = nullptr;
};

} // namespace L0
