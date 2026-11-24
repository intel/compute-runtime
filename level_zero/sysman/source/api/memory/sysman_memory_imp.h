/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/memory/sysman_memory.h"
#include "level_zero/sysman/source/api/memory/sysman_os_memory.h"
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {
class OsMemory;
struct OsSysman;

class MemoryImp : public Memory, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t memoryGetProperties(zes_mem_properties_t *pProperties) override;
    ze_result_t memoryGetBandwidth(zes_mem_bandwidth_t *pBandwidth) override;
    ze_result_t memoryGetState(zes_mem_state_t *pState) override;
    ze_result_t memoryGetBandwidthSample(size_t sampleSize, void *pSample) override;
    ze_result_t memoryCalculateBandwidthFromSamples(size_t sampleSize, const void *pSampleStart, const void *pSampleEnd, zes_intel_mem_bandwidth_exp_t *pBandwidth) override;

    MemoryImp(OsSysman *pOsSysman, bool onSubdevice, uint32_t subDeviceId);
    ~MemoryImp() override;

    MemoryImp() = default;
    void init();
    std::unique_ptr<OsMemory> pOsMemory;

  private:
    zes_mem_properties_t memoryProperties = {};
};

} // namespace Sysman
} // namespace L0
