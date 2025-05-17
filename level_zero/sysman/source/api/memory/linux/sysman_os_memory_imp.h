/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/memory/sysman_os_memory.h"

#include <string>

namespace NEO {
class Drm;
}

namespace L0 {
namespace Sysman {

class LinuxSysmanImp;
class SysmanKmdInterface;
struct SysmanDeviceImp;

class LinuxMemoryImp : public OsMemory, NEO::NonCopyableAndNonMovableClass {
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

  private:
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    std::string physicalSizeFile = "";
};

} // namespace Sysman
} // namespace L0
