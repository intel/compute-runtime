/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/platform/platform.h"

#include "gmm_client_context.h"
#include "gmm_memory.h"
#include "gtest/gtest.h"

using namespace NEO;
class PublicGmmMemory : public GmmMemory {
  public:
    using GmmMemory::clientContext;
};

TEST(GmmMemoryTest, givenGmmHelperWhenCreateGmmMemoryThenItHasClientContextFromGmmHelper) {
    ASSERT_NE(nullptr, platform()->peekGmmClientContext());
    PublicGmmMemory gmmMemory;
    EXPECT_EQ(gmmMemory.clientContext, platform()->peekGmmClientContext()->getHandle());
}
