/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_deferrable_deletion.h"

#include "gtest/gtest.h"

namespace NEO {
bool MockDeferrableDeletion::apply() {
    applyCalled++;
    return true;
}
MockDeferrableDeletion::~MockDeferrableDeletion() {
    EXPECT_EQ(1, applyCalled);
}
} // namespace NEO