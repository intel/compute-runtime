/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_deferrable_deletion.h"

namespace NEO {
bool MockDeferrableDeletion::apply() {
    applyCalled++;
    return true;
}
MockDeferrableDeletion::~MockDeferrableDeletion() {
    EXPECT_EQ(1, applyCalled);
}
} // namespace NEO