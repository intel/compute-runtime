/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/tools/source/sysman/memory/memory.h"
#include "level_zero/tools/source/sysman/memory/os_memory.h"
#include <level_zero/zet_api.h>

namespace L0 {

class MemoryImp : public Memory {
  public:
    ze_result_t memoryGetProperties(zet_mem_properties_t *pProperties) override;
    ze_result_t memoryGetBandwidth(zet_mem_bandwidth_t *pBandwidth) override;
    ze_result_t memoryGetState(zet_mem_state_t *pState) override;

    MemoryImp(OsSysman *pOsSysman, ze_device_handle_t hDevice);
    ~MemoryImp() override;

    MemoryImp(const MemoryImp &obj) = delete;
    MemoryImp &operator=(const MemoryImp &obj) = delete;

  private:
    OsMemory *pOsMemory;
    zet_mem_properties_t memoryProperties = {};
    void init();
    ze_device_handle_t hCoreDevice;
};

} // namespace L0
