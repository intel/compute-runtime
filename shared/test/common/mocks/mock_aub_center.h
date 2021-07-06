/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/aub/aub_center.h"
#include "shared/source/aub/aub_stream_provider.h"
#include "shared/test/common/mocks/mock_aub_file_stream.h"

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
    using AubCenter::stepping;

    MockAubCenter() {
        streamProvider.reset(new MockAubStreamProvider());
    }

    ~MockAubCenter() override = default;
};
} // namespace NEO
