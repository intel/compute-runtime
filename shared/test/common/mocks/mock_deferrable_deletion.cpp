/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_deferrable_deletion.h"

namespace NEO {
bool MockDeferrableDeletion::apply() {
    applyCalled++;
    return true;
}
MockDeferrableDeletion::~MockDeferrableDeletion() {
    EXPECT_EQ(1, applyCalled);
}
} // namespace NEO