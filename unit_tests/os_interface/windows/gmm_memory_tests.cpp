/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"

#include "gmm_client_context.h"
#include "gmm_memory.h"
#include "gtest/gtest.h"

using namespace OCLRT;
class PublicGmmMemory : public GmmMemory {
  public:
    using GmmMemory::clientContext;
};

TEST(GmmMemoryTest, givenGmmHelperWhenCreateGmmMemoryThenItHasClientContextFromGmmHelper) {
    ASSERT_NE(nullptr, GmmHelper::getClientContext());
    PublicGmmMemory gmmMemory;
    EXPECT_EQ(gmmMemory.clientContext, GmmHelper::getClientContext()->getHandle());
}
