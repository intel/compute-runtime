/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/aub/aub_center.h"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/test/common/mocks/mock_aub_file_stream.h"

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
};
} // namespace NEO
