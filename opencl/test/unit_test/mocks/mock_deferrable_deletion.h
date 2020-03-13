/*
 * Copyright (C) 2017-2020 Intel Corporation
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
    int applyCalled = 0;
};
} // namespace NEO
