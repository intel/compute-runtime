/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/windows/wddm_fixture.h"

namespace NEO {

using WddmTimestampPageTests = WddmFixture;

TEST_F(WddmTimestampPageTests, whenCreatingMmioTimestampPtrHelperThenItIsNotAvailable) {
    EXPECT_FALSE(wddm->createMmioTimestampPtrHelper(osContext->getWddmContextHandle()).isAvailable());
}

} // namespace NEO
