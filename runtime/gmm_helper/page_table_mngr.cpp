/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/page_table_mngr.h"

namespace NEO {
GmmPageTableMngr *GmmPageTableMngr::create(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    return new GmmPageTableMngr(translationTableFlags, translationTableCb);
}

} // namespace NEO
