/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/memory/memory.h"
#include "level_zero/tools/source/sysman/memory/os_memory.h"
#include <level_zero/zes_api.h>

namespace L0 {

class MemoryImp : public Memory, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t memoryGetProperties(zes_mem_properties_t *pProperties) override;
    ze_result_t memoryGetBandwidth(zes_mem_bandwidth_t *pBandwidth) override;
    ze_result_t memoryGetState(zes_mem_state_t *pState) override;

    MemoryImp(OsSysman *pOsSysman, ze_device_handle_t handle);
    ~MemoryImp() override;

    MemoryImp() = default;
    void init();
    std::unique_ptr<OsMemory> pOsMemory;

  private:
    zes_mem_properties_t memoryProperties = {};
};

} // namespace L0
