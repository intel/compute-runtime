/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/deferrable_deletion.h"

namespace NEO {
class MockDeferrableDeletion : public DeferrableDeletion {
  public:
    bool apply() override;

    ~MockDeferrableDeletion() override;
    int applyCalled = 0;
};
} // namespace NEO
