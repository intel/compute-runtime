/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/debug_helpers.h"

namespace NEO {
GmmPageTableMngr::GmmPageTableMngr(GmmClientContext *gmmClientContext, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb, void *aubCsrHandle) {
    auto clientContext = gmmClientContext->getHandle();
    auto deleter = [=](GMM_PAGETABLE_MGR *pageTableManager) {
        clientContext->DestroyPageTblMgrObject(pageTableManager);
    };

    pageTableManager = {clientContext->CreatePageTblMgrObject(translationTableFlags), deleter};
    DEBUG_BREAK_IF(pageTableManager == nullptr);
}
} // namespace NEO
