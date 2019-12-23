/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/gmm_helper/page_table_mngr.h"

#include "gmock/gmock.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

void dummySetCsrHandle(GMM_PAGETABLE_MGR *, HANDLE);

namespace NEO {
class MockGmmPageTableMngr : public GmmPageTableMngr {
  public:
    using GmmPageTableMngr::csrHandle;
    MockGmmPageTableMngr() : MockGmmPageTableMngr(0u, nullptr){};

    MockGmmPageTableMngr(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb)
        : translationTableFlags(translationTableFlags) {
        if (translationTableCb) {
            this->translationTableCb = *translationTableCb;
        }
        gmmSetCsrHandleFunc = dummySetCsrHandle;
    };

    MOCK_METHOD2(initContextAuxTableRegister, GMM_STATUS(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType));

    MOCK_METHOD1(updateAuxTable, GMM_STATUS(const GMM_DDI_UPDATEAUXTABLE *ddiUpdateAuxTable));

    void setCsrHandle(void *csrHandle) override;

    uint32_t setCsrHanleCalled = 0;

    unsigned int translationTableFlags = 0;
    GMM_TRANSLATIONTABLE_CALLBACKS translationTableCb = {};
};

} // namespace NEO

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
