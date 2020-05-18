/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/page_table_mngr.h"

#include "gmock/gmock.h"

namespace NEO {
class MockGmmPageTableMngr : public GmmPageTableMngr {
  public:
    MockGmmPageTableMngr() = default;

    MockGmmPageTableMngr(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb)
        : translationTableFlags(translationTableFlags) {
        if (translationTableCb) {
            this->translationTableCb = *translationTableCb;
        }
    };

    MOCK_METHOD(GMM_STATUS, initContextAuxTableRegister, (HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType), (override));

    MOCK_METHOD(GMM_STATUS, updateAuxTable, (const GMM_DDI_UPDATEAUXTABLE *ddiUpdateAuxTable), (override));

    void setCsrHandle(void *csrHandle) override;

    uint32_t setCsrHanleCalled = 0;
    void *passedCsrHandle = nullptr;

    unsigned int translationTableFlags = 0;
    GMM_TRANSLATIONTABLE_CALLBACKS translationTableCb = {};
};

} // namespace NEO
