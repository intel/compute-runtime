/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"

namespace NEO {
using namespace ::testing;

GmmPageTableMngr *GmmPageTableMngr::create(GmmClientContext *clientContext, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    auto pageTableMngr = new ::testing::NiceMock<MockGmmPageTableMngr>(translationTableFlags, translationTableCb);
    ON_CALL(*pageTableMngr, initContextAuxTableRegister(_, _)).WillByDefault(Return(GMM_SUCCESS));
    ON_CALL(*pageTableMngr, updateAuxTable(_)).WillByDefault(Return(GMM_SUCCESS));
    return pageTableMngr;
}
void MockGmmPageTableMngr::setCsrHandle(void *csrHandle) {
    passedCsrHandle = csrHandle;
    setCsrHanleCalled++;
}
} // namespace NEO
