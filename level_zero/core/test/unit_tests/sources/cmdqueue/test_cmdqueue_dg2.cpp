/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {
using CommandQueueTestDG2 = Test<DeviceFixture>;

HWTEST2_F(CommandQueueTestDG2, givenBindlessEnabledWhenEstimateStateBaseAddressCmdSizeCalledOnDG2ThenReturnedSizeOf2SBAAndPCAnd3DBindingTablePoolPool, IsDG2) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = std::make_unique<MockCommandQueueHw<gfxCoreFamily>>(device, csr.get(), &desc);
    auto size = commandQueue->estimateStateBaseAddressCmdSize();
    auto expectedSize = 2 * sizeof(STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL) + sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC);
    EXPECT_EQ(size, expectedSize);
}

} // namespace ult
} // namespace L0
