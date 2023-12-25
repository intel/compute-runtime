/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/sysman/source/api/memory/sysman_os_memory.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace NEO {
class Drm;
}

namespace L0 {
namespace Sysman {

class SysmanKmdInterface;
class PlatformMonitoringTech;
class LinuxSysmanImp;

class LinuxMemoryImp : public OsMemory, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_mem_properties_t *pProperties) override;
    ze_result_t getBandwidth(zes_mem_bandwidth_t *pBandwidth) override;
    ze_result_t getState(zes_mem_state_t *pState) override;
    bool isMemoryModuleSupported() override;
    LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxMemoryImp() = default;
    ~LinuxMemoryImp() override = default;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    NEO::Drm *pDrm = nullptr;
    SysmanDeviceImp *pDevice = nullptr;
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;
    PlatformMonitoringTech *pPmt = nullptr;

  private:
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    std::string physicalSizeFile = "";
};

} // namespace Sysman
} // namespace L0
