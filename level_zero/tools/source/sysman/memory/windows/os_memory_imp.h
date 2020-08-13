/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/memory/os_memory.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {
class KmdSysManager;

constexpr uint32_t MbpsToBytesPerSecond = 125000;

class WddmMemoryImp : public OsMemory, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_mem_properties_t *pProperties) override;
    ze_result_t getBandwidth(zes_mem_bandwidth_t *pBandwidth) override;
    ze_result_t getState(zes_mem_state_t *pState) override;
    bool isMemoryModuleSupported() override;
    WddmMemoryImp(OsSysman *pOsSysman);
    WddmMemoryImp() = default;
    ~WddmMemoryImp() override = default;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    Device *pDevice = nullptr;
};

} // namespace L0
