/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {
using CommandQueueTestDG2 = Test<DeviceFixture>;

HWTEST2_F(CommandQueueTestDG2, givenBindlessEnabledWhenEstimateStateBaseAddressCmdSizeCalledOnDG2ThenReturnedSizeIsZero, IsDG2) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseBindlessMode.set(1);
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr.get(), &desc);
    auto size = commandQueue->estimateStateBaseAddressCmdSize();
    auto expectedSize = 0u;

    EXPECT_EQ(size, expectedSize);
}

} // namespace ult
} // namespace L0
