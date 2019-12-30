/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/page_table_mngr.h"

namespace NEO {
GmmPageTableMngr *GmmPageTableMngr::create(unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    return new GmmPageTableMngr(translationTableFlags, translationTableCb);
}

} // namespace NEO
