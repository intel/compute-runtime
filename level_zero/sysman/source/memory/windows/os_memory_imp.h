/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/memory/os_memory.h"

namespace L0 {
namespace Sysman {
class WddmMemoryImp : public OsMemory, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_mem_properties_t *pProperties) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t getBandwidth(zes_mem_bandwidth_t *pBandwidth) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    ze_result_t getState(zes_mem_state_t *pState) override { return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; }
    bool isMemoryModuleSupported() override { return false; }
    WddmMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {}
    WddmMemoryImp() = default;
    ~WddmMemoryImp() {}
};

} // namespace Sysman
} // namespace L0
