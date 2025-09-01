/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/fixtures/aub_fixtures/multicontext_aub_fixture.h"

#include "opencl/source/command_queue/command_queue.h"

#include <memory>
#include <vector>

namespace NEO {
class MockClDevice;
class ClDevice;
class MockContext;
class CommandQueue;

struct MulticontextOclAubFixture : public MulticontextAubFixture {
    void setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool enableCompression);

    CommandStreamReceiver *getGpgpuCsr(uint32_t tile, uint32_t engine) override;
    void createDevices(const HardwareInfo &hwInfo, uint32_t numTiles) override;

    std::vector<ClDevice *> tileDevices;
    MockClDevice *rootDevice;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<MockContext> multiTileDefaultContext;
    std::vector<std::vector<std::unique_ptr<CommandQueue>>> commandQueues;
};
} // namespace NEO
