/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"

class MockAubCenter : public OCLRT::AubCenter {
  public:
    using AubCenter::AubCenter;
    using AubCenter::aubManager;
    ~MockAubCenter() override = default;
};
