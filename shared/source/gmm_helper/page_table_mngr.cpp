/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/page_table_mngr.h"

namespace NEO {
GmmPageTableMngr *GmmPageTableMngr::create(GmmClientContext *clientContext, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    return new GmmPageTableMngr(clientContext, translationTableFlags, translationTableCb);
}

} // namespace NEO
