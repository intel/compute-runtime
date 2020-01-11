/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/program/program_info.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(ProgramInfoTests, whenPrepareLinkerInputStorageGetsCalledTwiceThenLinkerInputStorageIsReused) {
    NEO::ProgramInfo programInfo;
    EXPECT_EQ(nullptr, programInfo.linkerInput);
    programInfo.prepareLinkerInputStorage();
    EXPECT_NE(nullptr, programInfo.linkerInput);
    auto prevLinkerInput = programInfo.linkerInput.get();
    programInfo.prepareLinkerInputStorage();
    EXPECT_EQ(prevLinkerInput, programInfo.linkerInput.get());
}
