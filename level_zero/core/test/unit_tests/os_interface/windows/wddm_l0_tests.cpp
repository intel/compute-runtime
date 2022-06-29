/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

TEST_F(WddmTestWithMockGdiDll, givenWddmWhenContextCreatedThenHintPassedIsOneApiL0) {
    init();
    auto createContextParams = getCreateContextDataFcn();
    EXPECT_EQ(D3DKMT_CLIENTHINT_ONEAPI_LEVEL0, createContextParams->ClientHint);
}
} // namespace NEO