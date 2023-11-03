/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/gmm_lib.h"

#include "level_zero/sysman/source/shared/windows/sysman_kmd_sys.h"
#include "level_zero/ze_api.h"

#include <vector>

namespace NEO {
class Wddm;
}

namespace L0 {
namespace Sysman {
class KmdSysManager {
  public:
    static KmdSysManager *create(NEO::Wddm *pWddm);
    KmdSysManager() = default;
    virtual ~KmdSysManager() = default;

    virtual ze_result_t requestSingle(KmdSysman::RequestProperty &In, KmdSysman::ResponseProperty &Out);
    virtual ze_result_t requestMultiple(std::vector<KmdSysman::RequestProperty> &vIn, std::vector<KmdSysman::ResponseProperty> &vOut);
    NEO::Wddm *GetWddmAccess() { return pWddmAccess; }

  private:
    virtual NTSTATUS escape(uint32_t escapeOp, uint64_t pDataIn, uint32_t dataInSize, uint64_t pDataOut, uint32_t dataOutSize);

    bool parseBufferIn(KmdSysman::GfxSysmanMainHeaderIn *pIn, std::vector<KmdSysman::RequestProperty> &vIn);
    bool parseBufferOut(KmdSysman::GfxSysmanMainHeaderOut *pOut, std::vector<KmdSysman::ResponseProperty> &vOut);

    KmdSysManager(NEO::Wddm *pWddm);
    NEO::Wddm *pWddmAccess = nullptr;
};
} // namespace Sysman
} // namespace L0
