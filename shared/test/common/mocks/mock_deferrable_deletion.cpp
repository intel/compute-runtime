/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_deferrable_deletion.h"

namespace NEO {
bool MockDeferrableDeletion::apply() {
    applyCalled++;
    return (applyCalled < trialTimes) ? false : true;
}
MockDeferrableDeletion::~MockDeferrableDeletion() {
    EXPECT_EQ(trialTimes, applyCalled);
}
} // namespace NEO