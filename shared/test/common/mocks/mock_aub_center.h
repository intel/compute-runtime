/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/aub/aub_center.h"

namespace NEO {

class MockAubCenter : public AubCenter {
  public:
    using AubCenter::AubCenter;
    using AubCenter::aubManager;
    using AubCenter::aubStreamMode;
    using AubCenter::stepping;

    MockAubCenter() {
    }

    ~MockAubCenter() override = default;

    std::unique_lock<std::mutex> obtainPageTablesLock() override {
        obtainPageTablesLockCalled++;
        return AubCenter::obtainPageTablesLock();
    }

    uint32_t obtainPageTablesLockCalled = 0u;
};
} // namespace NEO
