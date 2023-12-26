/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/sysman/source/api/memory/sysman_os_memory.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"

#include <map>

namespace L0 {
namespace Sysman {

class SysFsAccessInterface;
struct Device;
class PlatformMonitoringTech;
class LinuxSysmanImp;
class SysmanKmdInterface;

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
    SysFsAccessInterface *pSysfsAccess = nullptr;
    NEO::Drm *pDrm = nullptr;
    SysmanDeviceImp *pDevice = nullptr;
    PlatformMonitoringTech *pPmt = nullptr;
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;

  private:
    static const std::string deviceMemoryHealth;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    std::string physicalSizeFile;
};

} // namespace Sysman
} // namespace L0
