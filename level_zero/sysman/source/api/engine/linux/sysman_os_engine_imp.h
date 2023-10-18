/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/sysman/source/api/engine/sysman_os_engine.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"

#include <unistd.h>

namespace L0 {
namespace Sysman {

class SysmanKmdInterface;
class PmuInterface;
struct Device;
class LinuxEngineImp : public OsEngine, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getActivity(zes_engine_stats_t *pStats) override;
    ze_result_t getActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t getProperties(zes_engine_properties_t &properties) override;
    bool isEngineModuleSupported() override;
    static zes_engine_group_t getGroupFromEngineType(zes_engine_group_t type);
    LinuxEngineImp() = default;
    LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubDevice);
    ~LinuxEngineImp() override {
        if (fd != -1) {
            close(static_cast<int>(fd));
            fd = -1;
        }
    }

  protected:
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;
    zes_engine_group_t engineGroup = ZES_ENGINE_GROUP_ALL;
    uint32_t engineInstance = 0;
    PmuInterface *pPmuInterface = nullptr;
    NEO::Drm *pDrm = nullptr;
    SysmanDeviceImp *pDevice = nullptr;
    uint32_t subDeviceId = 0;
    ze_bool_t onSubDevice = false;

  private:
    void init();
    int64_t fd = -1;
};

} // namespace Sysman
} // namespace L0
