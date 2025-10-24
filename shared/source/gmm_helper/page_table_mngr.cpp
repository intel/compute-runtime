/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/page_table_mngr.h"

namespace NEO {
GmmPageTableMngr *GmmPageTableMngr::create(GmmClientContext *clientContext, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb, void *aubCsrHandle) {
    return new GmmPageTableMngr(clientContext, translationTableFlags, translationTableCb, aubCsrHandle);
}

} // namespace NEO
