/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"

namespace NEO {

GmmPageTableMngr *GmmPageTableMngr::create(GmmClientContext *clientContext, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb, void *aubCsrHandle) {
    auto pageTableMngr = new MockGmmPageTableMngr(translationTableFlags, translationTableCb, aubCsrHandle);
    return pageTableMngr;
}
} // namespace NEO
