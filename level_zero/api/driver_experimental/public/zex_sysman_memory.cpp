/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/zex_sysman_memory.h"

#include "level_zero/api/driver_experimental/public/zex_api.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/sysman/source/sysman_device.h"
#include "level_zero/sysman/source/sysman_driver.h"
#include "level_zero/sysman/source/sysman_driver_handle.h"
#include "level_zero/tools/source/sysman/memory/memory.h"
#include "level_zero/tools/source/sysman/sysman.h"

namespace L0 {

ze_result_t ZE_APICALL
zexSysmanMemoryGetBandwidth(
    zes_mem_handle_t hMemory,
    uint64_t *pReadCounters,
    uint64_t *pWriteCounters,
    uint64_t *pMaxBandwidth,
    uint64_t timeout) {
    if (L0::sysmanInitFromCore) {
        return L0::Memory::fromHandle(hMemory)->memoryGetBandwidthEx(pReadCounters, pWriteCounters, pMaxBandwidth, timeout);
    }
    return L0::Sysman::Memory::fromHandle(hMemory)->memoryGetBandwidthEx(pReadCounters, pWriteCounters, pMaxBandwidth, timeout);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexSysmanMemoryGetBandwidth(
    zes_mem_handle_t hMemory,
    uint64_t *pReadCounters,
    uint64_t *pWriteCounters,
    uint64_t *pMaxBandwidth,
    uint64_t timeout) {
    return L0::zexSysmanMemoryGetBandwidth(hMemory, pReadCounters, pWriteCounters, pMaxBandwidth, timeout);
}
}
