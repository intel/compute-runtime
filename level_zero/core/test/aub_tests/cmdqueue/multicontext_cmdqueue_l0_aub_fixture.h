/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/test/aub_tests/fixtures/multicontext_l0_aub_fixture.h"
#include <level_zero/ze_api.h>

#include <cstdint>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
enum class EngineGroupType : uint32_t;
} // namespace NEO

namespace L0 {
struct Device;

namespace ult {

struct MulticontextCmdQueueL0AubFixture : public MulticontextL0AubFixture {
    struct QueueEntry {
        ze_command_queue_handle_t queue = nullptr;
        ze_command_list_handle_t commandList = nullptr;
        NEO::CommandStreamReceiver *csr = nullptr;
    };

    void setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool implicitScaling);
    void tearDown();

    NEO::CommandStreamReceiver *getGpgpuCsr(uint32_t tile, uint32_t engine) override;

    L0::Device *getTileDevice(uint32_t tile) const;
    QueueEntry createQueue(L0::Device *device, NEO::EngineGroupType engineGroupType, uint32_t engineIndex);
    void *allocDevice(L0::Device *device, size_t size);
    void *allocHost(size_t size);
    void closeAndExecute(const QueueEntry &entry);
    void synchronize(const QueueEntry &entry);

    std::vector<std::vector<QueueEntry>> queues;
    std::vector<QueueEntry> additionalQueues;
    std::vector<void *> allocations;
    ze_context_handle_t contextHandle = nullptr;
};

} // namespace ult
} // namespace L0
