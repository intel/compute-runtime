/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/memory/os_memory.h"

namespace L0 {

class WddmMemoryImp : public OsMemory {

  public:
    ze_result_t getMemorySize(uint64_t &maxSize, uint64_t &allocSize) override;
    ze_result_t getMemHealth(zet_mem_health_t &memHealth) override;
};

ze_result_t WddmMemoryImp::getMemorySize(uint64_t &maxSize, uint64_t &allocSize) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmMemoryImp::getMemHealth(zet_mem_health_t &memHealth) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsMemory *OsMemory::create(OsSysman *pOsSysman) {
    WddmMemoryImp *pWddmMemoryImp = new WddmMemoryImp();
    return static_cast<OsMemory *>(pWddmMemoryImp);
}

} // namespace L0
