/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/engine/os_engine.h"

namespace L0 {

class WddmEngineImp : public OsEngine {
  public:
    ze_result_t getActiveTime(uint64_t &activeTime) override;
};

ze_result_t WddmEngineImp::getActiveTime(uint64_t &activeTime) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsEngine *OsEngine::create(OsSysman *pOsSysman) {
    WddmEngineImp *pWddmEngineImp = new WddmEngineImp();
    return static_cast<OsEngine *>(pWddmEngineImp);
}

} // namespace L0
