/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(OSContext, whenCreatingDefaultOsContextThenExpectInitializedAlways) {
    OsContext *osContext = OsContext::create(nullptr, 0, 0, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    EXPECT_TRUE(osContext->isInitialized());
    delete osContext;
}
