/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/sip_external_lib/sip_external_lib.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(SipExternalLibTest, whenGetSipExternalLibInstanceCalledThenNullptrRetuned) {
    auto sipExternalLib = SipExternalLib::getSipExternalLibInstance();
    EXPECT_EQ(nullptr, sipExternalLib);
}