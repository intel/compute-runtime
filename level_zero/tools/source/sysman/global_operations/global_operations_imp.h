/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <level_zero/zet_api.h>

#include "global_operations.h"
#include "os_global_operations.h"

#include <mutex>
#include <vector>

namespace L0 {

class GlobalOperationsImp : public GlobalOperations, NEO::NonCopyableOrMovableClass {
  public:
    void init() override;
    ze_result_t reset(ze_bool_t force) override;
    ze_result_t deviceGetProperties(zes_device_properties_t *pProperties) override;
    ze_result_t processesGetState(uint32_t *pCount, zes_process_state_t *pProcesses) override;
    ze_result_t deviceGetState(zes_device_state_t *pState) override;
    OsGlobalOperations *pOsGlobalOperations = nullptr;

    GlobalOperationsImp() = default;
    GlobalOperationsImp(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~GlobalOperationsImp() override;

  private:
    OsSysman *pOsSysman = nullptr;
    zes_device_properties_t sysmanProperties = {};
    std::once_flag initGlobalOpOnce;
    void initGlobalOperations();
};

} // namespace L0
