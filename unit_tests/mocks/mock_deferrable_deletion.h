/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/deferrable_deletion.h"

#include "gtest/gtest.h"

namespace OCLRT {
class MockDeferrableDeletion : public DeferrableDeletion {
  public:
    bool apply() override;

    virtual ~MockDeferrableDeletion();
    int applyCalled = 0;
};
} // namespace OCLRT