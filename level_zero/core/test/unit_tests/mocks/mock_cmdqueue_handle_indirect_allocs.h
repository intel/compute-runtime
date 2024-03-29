/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandQueueHandleIndirectAllocs : public MockCommandQueueHw<gfxCoreFamily> {
  public:
    using typename MockCommandQueueHw<gfxCoreFamily>::CommandListExecutionContext;
    using MockCommandQueueHw<gfxCoreFamily>::executeCommandListsRegular;
    using MockCommandQueueHw<gfxCoreFamily>::executeCommandListsRegularHeapless;

    MockCommandQueueHandleIndirectAllocs(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : MockCommandQueueHw<gfxCoreFamily>(device, csr, desc) {}
    void handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration) override {
        handleIndirectAllocationResidencyCalledTimes++;
        MockCommandQueueHw<gfxCoreFamily>::handleIndirectAllocationResidency(unifiedMemoryControls, lockForIndirect, performMigration);
    }
    void makeResidentAndMigrate(bool performMigration, const NEO::ResidencyContainer &residencyContainer) override {
        makeResidentAndMigrateCalledTimes++;
    }
    uint32_t handleIndirectAllocationResidencyCalledTimes = 0;
    uint32_t makeResidentAndMigrateCalledTimes = 0;
};

} // namespace ult
} // namespace L0
