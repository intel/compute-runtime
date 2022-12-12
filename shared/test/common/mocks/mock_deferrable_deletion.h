/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/deferrable_deletion.h"

#include "gtest/gtest.h"

namespace NEO {
class MockDeferrableDeletion : public DeferrableDeletion {
  public:
    bool apply() override;

    ~MockDeferrableDeletion() override;

    void setTrialTimes(int times) { trialTimes = times; }
    int applyCalled = 0;

  private:
    int trialTimes = 1;
};
} // namespace NEO
