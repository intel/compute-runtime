/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hw_info.h"

#include "gtest/gtest.h"

namespace NEO {
class UltConfigListener : public ::testing::EmptyTestEventListener {
  private:
    void OnTestStart(const ::testing::TestInfo &) override;
    void OnTestEnd(const ::testing::TestInfo &) override;
    HardwareInfo referencedHwInfo;
};
} // namespace NEO
