/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gtest/gtest.h"

namespace NEO {

// capture allocations call stacks to print them during memory leak in ULTs
constexpr bool captureCallStacks = false;

class MemoryLeakListener : public ::testing::EmptyTestEventListener {
  private:
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;

    unsigned int numInitialBaseObjects;
};
} // namespace NEO
