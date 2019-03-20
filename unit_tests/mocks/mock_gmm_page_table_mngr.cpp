/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_gmm_page_table_mngr.h"

namespace OCLRT {
using namespace ::testing;

GmmPageTableMngr *GmmPageTableMngr::create(GMM_DEVICE_CALLBACKS_INT *deviceCb, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    auto pageTableMngr = new ::testing::NiceMock<MockGmmPageTableMngr>(deviceCb, translationTableFlags, translationTableCb);
    ON_CALL(*pageTableMngr, initContextAuxTableRegister(_, _)).WillByDefault(Return(GMM_SUCCESS));
    ON_CALL(*pageTableMngr, initContextTRTableRegister(_, _)).WillByDefault(Return(GMM_SUCCESS));
    ON_CALL(*pageTableMngr, updateAuxTable(_)).WillByDefault(Return(GMM_SUCCESS));
    return pageTableMngr;
}
} // namespace OCLRT
