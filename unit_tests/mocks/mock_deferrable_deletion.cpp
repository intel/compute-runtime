/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_deferrable_deletion.h"

namespace OCLRT {
void MockDeferrableDeletion::apply() {
    applyCalled++;
}
MockDeferrableDeletion::~MockDeferrableDeletion() {
    EXPECT_EQ(1, applyCalled);
}
} // namespace OCLRT