/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/gmm_helper/page_table_mngr.h"

#include "gmock/gmock.h"

namespace NEO {
class MockGmmPageTableMngr : public GmmPageTableMngr {
  public:
    MockGmmPageTableMngr() = default;

    MockGmmPageTableMngr(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb)
        : translationTableFlags(translationTableFlags), translationTableCb(*translationTableCb){};

    MOCK_METHOD2(initContextAuxTableRegister, GMM_STATUS(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType));

    MOCK_METHOD2(initContextTRTableRegister, GMM_STATUS(HANDLE initialBBHandle, GMM_ENGINE_TYPE engineType));

    MOCK_METHOD1(updateAuxTable, GMM_STATUS(const GMM_DDI_UPDATEAUXTABLE *ddiUpdateAuxTable));

    void setCsrHandle(void *csrHandle) override;

    uint32_t setCsrHanleCalled = 0;
    void *passedCsrHandle = nullptr;

    GMM_TRANSLATIONTABLE_CALLBACKS translationTableCb = {};
    unsigned int translationTableFlags = 0;
};

} // namespace NEO
