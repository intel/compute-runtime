/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/tools/source/sysman/windows/kmd_sys.h"
#include "level_zero/ze_api.h"
#include "level_zero/zet_api.h"

#include <string>
#include <vector>

namespace L0 {
class KmdSysManager {
  public:
    static KmdSysManager *create(NEO::Wddm *pWddm);
    KmdSysManager() = default;
    ~KmdSysManager() = default;

    MOCKABLE_VIRTUAL ze_result_t requestSingle(KmdSysman::RequestProperty &In, KmdSysman::ResponseProperty &Out);
    ze_result_t requestMultiple(std::vector<KmdSysman::RequestProperty> &vIn, std::vector<KmdSysman::ResponseProperty> &vOut);
    NEO::Wddm *GetWddmAccess() { return pWddmAccess; }

  private:
    MOCKABLE_VIRTUAL bool escape(uint32_t escapeOp, uint64_t pDataIn, uint32_t dataInSize, uint64_t pDataOut, uint32_t dataOutSize);

    bool parseBufferIn(KmdSysman::GfxSysmanMainHeaderIn *pIn, std::vector<KmdSysman::RequestProperty> &vIn);
    bool parseBufferOut(KmdSysman::GfxSysmanMainHeaderOut *pOut, std::vector<KmdSysman::ResponseProperty> &vOut);

    KmdSysManager(NEO::Wddm *pWddm);
    NEO::Wddm *pWddmAccess = nullptr;
};
} // namespace L0
