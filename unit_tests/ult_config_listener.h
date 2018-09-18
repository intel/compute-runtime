/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "gtest/gtest.h"

namespace OCLRT {
class UltConfigListener : public ::testing::EmptyTestEventListener {
  private:
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;
};
} // namespace OCLRT
