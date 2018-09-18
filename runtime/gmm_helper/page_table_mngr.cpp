/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/page_table_mngr.h"

namespace OCLRT {
GmmPageTableMngr *GmmPageTableMngr::create(GMM_DEVICE_CALLBACKS_INT *deviceCb, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    return new GmmPageTableMngr(deviceCb, translationTableFlags, translationTableCb);
}

} // namespace OCLRT
