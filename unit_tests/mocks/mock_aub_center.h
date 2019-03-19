/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/aub/aub_center.h"
#include "runtime/command_stream/aub_stream_provider.h"
#include "unit_tests/mocks/mock_aub_file_stream.h"

namespace NEO {
class MockAubStreamProvider : public AubStreamProvider {
  public:
    AubMemDump::AubFileStream *getStream() override {
        return &stream;
    }

  protected:
    MockAubFileStream stream;
};

class MockAubCenter : public AubCenter {
  public:
    using AubCenter::AubCenter;
    using AubCenter::aubManager;
    using AubCenter::aubStreamMode;

    MockAubCenter() {
        streamProvider.reset(new MockAubStreamProvider());
    }

    ~MockAubCenter() override = default;
};
} // namespace NEO
