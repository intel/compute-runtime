/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/page_table_mngr.h"

#include "gmm_client_context.h"

namespace NEO {
GmmPageTableMngr::~GmmPageTableMngr() {
    if (clientContext) {
        clientContext->DestroyPageTblMgrObject(pageTableManager);
    }
}

GmmPageTableMngr::GmmPageTableMngr(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    clientContext = GmmHelper::getClientContext()->getHandle();
    pageTableManager = clientContext->CreatePageTblMgrObject(translationTableCb, translationTableFlags);
}

} // namespace NEO
