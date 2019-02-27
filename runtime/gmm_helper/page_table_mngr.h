/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm_lib.h"

#include <functional>
#include <memory>

namespace OCLRT {
class GmmPageTableMngr {
  public:
    MOCKABLE_VIRTUAL ~GmmPageTableMngr();

    static GmmPageTableMngr *create(GMM_DEVICE_CALLBACKS_INT *deviceCb, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb);

    MOCKABLE_VIRTUAL GMM_STATUS initContextAuxTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType) {
        return pageTableManager->InitContextAuxTableRegister(initialBBHandle, engineType);
    }

    MOCKABLE_VIRTUAL GMM_STATUS initContextTRTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType) {
        return pageTableManager->InitContextTRTableRegister(initialBBHandle, engineType);
    }

    MOCKABLE_VIRTUAL GMM_STATUS updateAuxTable(const GMM_DDI_UPDATEAUXTABLE *ddiUpdateAuxTable) {
        return pageTableManager->UpdateAuxTable(ddiUpdateAuxTable);
    }

  protected:
    GmmPageTableMngr() = default;

    GmmPageTableMngr(GMM_DEVICE_CALLBACKS_INT *deviceCb, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb);
    GMM_CLIENT_CONTEXT *clientContext = nullptr;
    GMM_PAGETABLE_MGR *pageTableManager = nullptr;
};
} // namespace OCLRT
