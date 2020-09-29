/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/sysman_const.h"

#include "sysman/memory/os_memory.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {
class KmdSysManager;

class WddmMemoryImp : public OsMemory, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_mem_properties_t *pProperties) override;
    ze_result_t getBandwidth(zes_mem_bandwidth_t *pBandwidth) override;
    ze_result_t getState(zes_mem_state_t *pState) override;
    bool isMemoryModuleSupported() override;
    WddmMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    WddmMemoryImp() = default;
    ~WddmMemoryImp() override = default;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    Device *pDevice = nullptr;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
};

} // namespace L0
