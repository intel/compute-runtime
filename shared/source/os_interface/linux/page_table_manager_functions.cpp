/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"

namespace NEO {
GmmPageTableMngr::GmmPageTableMngr(GmmClientContext *gmmClientContext, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) : clientContext(gmmClientContext->getHandle()) {
    pageTableManager = clientContext->CreatePageTblMgrObject(translationTableFlags);
    DEBUG_BREAK_IF(pageTableManager == nullptr);
}

void GmmPageTableMngr::setCsrHandle(void *csrHandle) {}
} // namespace NEO
