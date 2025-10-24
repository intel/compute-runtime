/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "External/Common/GmmPageTableMgr.h"

#include <functional>
#include <memory>

namespace NEO {
class Gmm;
class GmmClientContext;
class LinearStream;
class GmmPageTableMngr : NonCopyableAndNonMovableClass {
  public:
    MOCKABLE_VIRTUAL ~GmmPageTableMngr() = default;

    static GmmPageTableMngr *create(GmmClientContext *clientContext, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb, void *aubCsrHandle);

    bool updateAuxTable(uint64_t gpuVa, Gmm *gmm, bool map);
    bool initPageTableManagerRegisters(void *csrHandle);

  protected:
    GmmPageTableMngr() = default;

    MOCKABLE_VIRTUAL GMM_STATUS updateAuxTable(const GMM_DDI_UPDATEAUXTABLE *ddiUpdateAuxTable) {
        return pageTableManager->UpdateAuxTable(ddiUpdateAuxTable);
    }

    MOCKABLE_VIRTUAL GMM_STATUS initContextAuxTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType) {
        return pageTableManager->InitContextAuxTableRegister(initialBBHandle, engineType);
    }

    GmmPageTableMngr(GmmClientContext *clientContext, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb, void *aubCsrHandle);
    std::unique_ptr<GMM_PAGETABLE_MGR, std::function<void(GMM_PAGETABLE_MGR *)>> pageTableManager;
};

static_assert(NonCopyableAndNonMovable<GmmPageTableMngr>);

} // namespace NEO
