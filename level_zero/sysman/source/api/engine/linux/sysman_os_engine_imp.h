/*
 * Copyright (C) 2020-2025 Intel Corporation
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
class LinuxSysmanImp;
struct Device;

struct EngineGroupInfo {
    zes_engine_group_t engineGroup;
    uint32_t engineInstance;
    uint32_t tileId;
};

class LinuxEngineImp : public OsEngine, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getActivity(zes_engine_stats_t *pStats) override;
    ze_result_t getActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) override {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t getProperties(zes_engine_properties_t &properties) override;
    bool isEngineModuleSupported() override;
    static zes_engine_group_t getGroupFromEngineType(zes_engine_group_t type);
    LinuxEngineImp() = default;
    LinuxEngineImp(OsSysman *pOsSysman, MapOfEngineInfo &mapEngineInfo, zes_engine_group_t type, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubDevice);
    ~LinuxEngineImp() override;
    void cleanup();

  protected:
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    EngineGroupInfo engineGroupInfo = {};
    PmuInterface *pPmuInterface = nullptr;
    NEO::Drm *pDrm = nullptr;
    SysmanDeviceImp *pDevice = nullptr;
    ze_bool_t onSubDevice = false;

  private:
    void init(MapOfEngineInfo &mapEngineInfo);
    std::vector<int64_t> fdList{};
    ze_result_t initStatus = ZE_RESULT_SUCCESS;
};

} // namespace Sysman
} // namespace L0
