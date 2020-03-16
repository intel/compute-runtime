/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/deferred_deleter_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DeferredDeleterHelper, GivenDefferedDeleterHelperWhenCheckIFDeferrDeleterIsEnabledThenFalseIsReturned) {
    EXPECT_FALSE(isDeferredDeleterEnabled());
}
