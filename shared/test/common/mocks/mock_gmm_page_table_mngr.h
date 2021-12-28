/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/utilities/stackvec.h"

namespace NEO {
class MockGmmPageTableMngr : public GmmPageTableMngr {
  public:
    MockGmmPageTableMngr() {
        initContextAuxTableRegisterParamsPassed.clear();
    };

    MockGmmPageTableMngr(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb)
        : translationTableFlags(translationTableFlags) {
        if (translationTableCb) {
            this->translationTableCb = *translationTableCb;
        }
    };

    GMM_STATUS initContextAuxTableRegister(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType) override {
        initContextAuxTableRegisterCalled++;
        initContextAuxTableRegisterParamsPassed.push_back({initialBBHandle, engineType});
        return initContextAuxTableRegisterResult;
    }

    struct InitContextAuxTableRegisterParams {
        HANDLE initialBBHandle = nullptr;
        GMM_ENGINE_TYPE engineType = GMM_ENGINE_TYPE::ENGINE_TYPE_RCS;
    };

    StackVec<InitContextAuxTableRegisterParams, 2> initContextAuxTableRegisterParamsPassed{};
    uint32_t initContextAuxTableRegisterCalled = 0u;
    GMM_STATUS initContextAuxTableRegisterResult = GMM_STATUS::GMM_SUCCESS;

    GMM_STATUS updateAuxTable(const GMM_DDI_UPDATEAUXTABLE *ddiUpdateAuxTable) override {
        updateAuxTableCalled++;
        updateAuxTableParamsPassed.push_back({*ddiUpdateAuxTable});
        return updateAuxTableResult;
    }

    struct UpdateAuxTableParams {
        GMM_DDI_UPDATEAUXTABLE ddiUpdateAuxTable = {};
    };

    StackVec<UpdateAuxTableParams, 1> updateAuxTableParamsPassed{};
    uint32_t updateAuxTableCalled = 0u;
    GMM_STATUS updateAuxTableResult = GMM_STATUS::GMM_SUCCESS;

    void setCsrHandle(void *csrHandle) override;

    uint32_t setCsrHanleCalled = 0;
    void *passedCsrHandle = nullptr;

    unsigned int translationTableFlags = 0;
    GMM_TRANSLATIONTABLE_CALLBACKS translationTableCb = {};
};

} // namespace NEO
