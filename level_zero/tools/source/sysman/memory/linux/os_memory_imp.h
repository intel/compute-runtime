/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/tools/source/sysman/memory/os_memory.h"

namespace L0 {

class SysfsAccess;
struct Device;

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
    NEO::Drm *pDrm = nullptr;
    Device *pDevice = nullptr;

  private:
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
};
} // namespace L0
