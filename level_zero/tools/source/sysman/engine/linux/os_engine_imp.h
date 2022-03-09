/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/tools/source/sysman/sysman_const.h"

#include "sysman/engine/os_engine.h"
namespace L0 {
class PmuInterface;
struct Device;
class LinuxEngineImp : public OsEngine, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getActivity(zes_engine_stats_t *pStats) override;
    ze_result_t getProperties(zes_engine_properties_t &properties) override;
    bool isEngineModuleSupported() override;
    static zes_engine_group_t getGroupFromEngineType(zes_engine_group_t type);
    LinuxEngineImp() = default;
    LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId);
    ~LinuxEngineImp() override {
        if (fd != -1) {
            close(static_cast<int>(fd));
            fd = -1;
        }
    }

  protected:
    zes_engine_group_t engineGroup = ZES_ENGINE_GROUP_ALL;
    uint32_t engineInstance = 0;
    PmuInterface *pPmuInterface = nullptr;
    NEO::Drm *pDrm = nullptr;
    Device *pDevice = nullptr;
    uint32_t subDeviceId = 0;
    uint32_t onSubDevice = 0;

  private:
    void init();
    int64_t fd = -1;
};

} // namespace L0
